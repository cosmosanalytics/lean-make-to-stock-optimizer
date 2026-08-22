"""Port of LeanMtsProblem.h/.cpp: the shared-board card allocation problem."""

from dataclasses import dataclass, field
from typing import List

from .kanban_sku import KanbanSku


@dataclass
class LeanMtsSolution:
    """A candidate card allocation produced by a solver: how many kanban
    cards each SKU gets, indexed identically to LeanMtsProblem.skus.
    """

    feasible: bool = True
    total_cost: float = 0.0
    cards_allocated: List[int] = field(default_factory=list)


class LeanMtsProblem:
    """A shared-board kanban card allocation problem: decide how many cards
    K_i each SKU gets (0 <= K_i <= card_cap_i) to minimize total annual
    cost, subject to a shared footprint budget (board slots / floor space)
    that no single SKU controls on its own. This is a separable
    resource-allocation problem -- specifically a multiple-choice knapsack,
    since exactly one card-count choice must be made per SKU.
    """

    def __init__(self, skus: List[KanbanSku], total_footprint_budget: float):
        self._skus = list(skus)
        self._total_footprint_budget = total_footprint_budget

    def skus(self) -> List[KanbanSku]:
        return self._skus

    def total_footprint_budget(self) -> float:
        return self._total_footprint_budget

    def validate(self, solution: LeanMtsSolution) -> None:
        """Recomputes feasibility and total_cost for a solution built by a
        solver: the allocation vector must have one entry per SKU, every
        K_i must fall within [0, card_cap_i], and total footprint used must
        not exceed the shared budget. Cost is the sum of each SKU's
        cost_for_cards(K_i), independent of how the allocation was produced.
        """
        if len(solution.cards_allocated) != len(self._skus):
            solution.feasible = False
            solution.total_cost = 0.0
            return

        feasible = True
        used_footprint = 0.0
        cost = 0.0
        for sku, k in zip(self._skus, solution.cards_allocated):
            if k < 0 or k > sku.card_cap():
                feasible = False
            used_footprint += sku.footprint_per_card * k
            cost += sku.cost_for_cards(k)

        if used_footprint > self._total_footprint_budget + 1e-9:
            feasible = False

        solution.feasible = feasible
        solution.total_cost = cost if feasible else 0.0
