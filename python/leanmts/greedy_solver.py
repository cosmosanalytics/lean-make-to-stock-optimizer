"""Port of GreedyMarginalCostSolver.h/.cpp."""

from .problem import LeanMtsProblem, LeanMtsSolution
from .solver import LeanMtsSolver


class GreedyMarginalCostSolver(LeanMtsSolver):
    """Fast heuristic: start every SKU at K=0 cards, then repeatedly grant
    one more card to whichever SKU currently offers the single largest cost
    reduction (steepest marginal-cost descent), subject to remaining
    footprint budget and each SKU's card_cap(). Stops when no remaining
    move improves total cost, or the budget is exhausted.

    NOT guaranteed optimal: each SKU's activation cost makes
    cost_for_cards() non-convex in K (the first card is disproportionately
    expensive because it's the only one that pays the fixed activation
    cost), which can trap a purely local, one-step-at-a-time greedy into a
    worse allocation than the global optimum. ExactDPSolver is the
    from-scratch solver that handles that non-convexity correctly.
    """

    def solve(self, problem: LeanMtsProblem) -> LeanMtsSolution:
        skus = problem.skus()

        solution = LeanMtsSolution()
        solution.cards_allocated = [0] * len(skus)

        remaining_budget = problem.total_footprint_budget()

        # Repeatedly take the single best next +1-card move across all SKUs
        # until nothing left improves cost or fits the remaining budget.
        while True:
            best_idx = -1
            best_saving = 1e-9  # require a strictly positive improvement

            for i, sku in enumerate(skus):
                k = solution.cards_allocated[i]
                if k >= sku.card_cap():
                    continue
                if sku.footprint_per_card > remaining_budget + 1e-9:
                    continue

                saving = sku.cost_for_cards(k) - sku.cost_for_cards(k + 1)
                if saving > best_saving:
                    best_saving = saving
                    best_idx = i

            if best_idx < 0:
                break  # no move helps -- local optimum reached

            solution.cards_allocated[best_idx] += 1
            remaining_budget -= skus[best_idx].footprint_per_card

        problem.validate(solution)
        return solution

    def name(self) -> str:
        return "Greedy-MarginalCost"
