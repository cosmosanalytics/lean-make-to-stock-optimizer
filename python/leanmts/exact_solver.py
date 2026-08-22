"""Port of ExactDPSolver.h/.cpp: bounded multiple-choice knapsack DP."""

import math
from typing import List

from .problem import LeanMtsProblem, LeanMtsSolution
from .solver import LeanMtsSolver

_INF_COST = math.inf


def _footprint_units(footprint_per_card: float, cards: int) -> int:
    """Rounds a real-valued footprint to the nearest whole budget unit
    (round-half-away-from-zero, matching C++'s std::llround). Board slots /
    floor positions are inherently discrete, so this is a faithful mapping
    onto the DP's integer state space for the non-negative inputs this
    project expects.
    """
    value = footprint_per_card * cards
    return math.floor(value + 0.5) if value >= 0 else -math.floor(-value + 0.5)


class ExactDPSolver(LeanMtsSolver):
    """Exact, dependency-free solver: a standard bounded multiple-choice
    knapsack dynamic program over the shared footprint budget.

    For each SKU i, cost_for_cards(k) is precomputed for every
    k = 0..card_cap_i (its "choices"). SKUs are processed one at a time;
    dp[used_budget] tracks the minimum total cost achievable after deciding
    the first several SKUs while consuming exactly `used_budget` footprint
    units. Because the DP tries every k for every SKU rather than assuming
    the cost curve is convex, it handles the fixed-activation-cost
    non-convexity correctly -- unlike GreedyMarginalCostSolver, which can
    get trapped by it.

    The budget is treated as an integral number of footprint units (whole
    board slots / floor positions); total_footprint_budget and each SKU's
    footprint_per_card are rounded to the nearest whole unit for DP
    indexing.

    Runtime is O(numSkus * budget * maxCardCap) -- pseudo-polynomial, like
    any knapsack DP, but exact and easily fast enough for the small/medium
    instances a portfolio demo or regression test uses. pulp_solver.py is
    the path for production scale / general (non-integral) footprints.
    """

    def solve(self, problem: LeanMtsProblem) -> LeanMtsSolution:
        skus = problem.skus()
        n = len(skus)

        result = LeanMtsSolution()
        if n == 0:
            result.feasible = True
            result.total_cost = 0.0
            return result

        budget = max(0, _footprint_units(problem.total_footprint_budget(), 1))

        # dp[used_budget] = minimum total cost of the SKUs decided so far,
        # consuming exactly used_budget footprint units.
        dp = [_INF_COST] * (budget + 1)
        dp[0] = 0.0

        # choice[i][used_budget_after_sku_i] = the card count chosen for
        # SKU i that achieves dp's value at that state, so the allocation
        # can be reconstructed by walking backwards once the DP completes.
        choice: List[List[int]] = [[-1] * (budget + 1) for _ in range(n)]

        for i, sku in enumerate(skus):
            cap = sku.card_cap()

            next_dp = [_INF_COST] * (budget + 1)
            for used in range(budget + 1):
                if dp[used] >= _INF_COST:
                    continue
                for k in range(cap + 1):
                    fp = _footprint_units(sku.footprint_per_card, k)
                    new_used = used + fp
                    if new_used > budget:
                        break  # fp is non-decreasing in k
                    new_cost = dp[used] + sku.cost_for_cards(k)
                    if new_cost < next_dp[new_used] - 1e-12:
                        next_dp[new_used] = new_cost
                        choice[i][new_used] = k
            dp = next_dp

        # Best total cost may leave some budget unused -- scan every state.
        best_state = 0
        best_cost = dp[0]
        for used in range(1, budget + 1):
            if dp[used] < best_cost:
                best_cost = dp[used]
                best_state = used

        result.cards_allocated = [0] * n
        if best_cost < _INF_COST:
            state = best_state
            for i in range(n - 1, -1, -1):
                k = choice[i][state]
                result.cards_allocated[i] = k
                state -= _footprint_units(skus[i].footprint_per_card, k)

        problem.validate(result)
        return result

    def name(self) -> str:
        return "ExactDP-Optimal"
