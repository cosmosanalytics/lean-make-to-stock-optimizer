"""Port of KanbanSku.h: one SKU replenished through a kanban pull lane."""

import math
from dataclasses import dataclass


@dataclass(frozen=True)
class KanbanSku:
    """One SKU on a shared kanban pull board.

    Cards are the unit of control: each card authorizes production/withdrawal
    of one container of `container_size` units, and the number of cards in
    circulation for a SKU is what actually determines how much buffer stock
    exists against demand during the replenishment lead time.
    """

    id: int
    name: str
    container_size: float
    demand_mean_per_day: float
    demand_std_dev_per_day: float
    lead_time_days: float
    holding_cost_per_unit_per_year: float
    shortage_cost_per_card_short: float
    activation_cost: float
    footprint_per_card: float
    service_level_z: float

    def required_cards(self) -> int:
        """Classical kanban card-count formula: enough containers to cover
        mean demand over the lead time, plus a safety-stock buffer sized off
        the demand standard deviation via the target service level's
        z-score, rounded up to a whole number of containers (you can't issue
        a fractional card).
        """
        demand_during_lead_time = self.demand_mean_per_day * self.lead_time_days
        safety_buffer = (
            self.service_level_z
            * self.demand_std_dev_per_day
            * math.sqrt(self.lead_time_days)
        )
        cards = (demand_during_lead_time + safety_buffer) / self.container_size
        return math.ceil(cards - 1e-9)

    def card_cap(self) -> int:
        """Practical upper bound on how many cards are ever worth
        allocating: one past the number needed to fully cover demand.
        Provisioning further than that never reduces cost (it only adds
        holding cost), so it's excluded from every solver's search space up
        front.
        """
        return self.required_cards() + 1

    def cost_for_cards(self, k: int) -> float:
        """Cost of operating this SKU's pull lane with exactly K cards on
        the shared board, for one year.

        K <= 0 means the SKU isn't stocked on the board at all: no
        activation cost, no holding cost, but the entire required buffer is
        "short" -- covered informally or not at all.

        K >= 1 means the lane is active: a fixed activation cost (printing
        cards, staging a point-of-use bin) plus holding cost on every unit
        carried across all K containers, plus a shortage penalty for any
        cards still short of the SKU's ideal required_cards().

        The fixed activation_cost for K >= 1 is what makes this function
        non-convex in K -- the first card is disproportionately expensive --
        which is exactly what makes the allocation problem across SKUs
        combinatorially interesting rather than a trivial per-SKU
        calculation.
        """
        required = self.required_cards()
        if k <= 0:
            return self.shortage_cost_per_card_short * required
        holding = self.holding_cost_per_unit_per_year * self.container_size * k
        shortage = self.shortage_cost_per_card_short * max(0, required - k)
        return self.activation_cost + holding + shortage
