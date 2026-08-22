"""Port of CbcMipSolver.h: multiple-choice-knapsack MIP via PuLP + CBC.

Mirrors the C++ project's documentary CbcMipSolver.h as closely as
possible: one binary y_{i,k} per (SKU, card-count) pair -- y_{i,k} = 1 iff
SKU i is allocated exactly k cards -- with exactly one k chosen per SKU and
the shared footprint budget enforced as a single knapsack row. Unlike the
C++ version (compiled only behind LEANMTS_USE_CBC), this is the default
production-scale path here since `pip install pulp` has no system
dependencies.

pulp is imported lazily inside solve() so importing the leanmts package
never requires it to be installed.
"""

from .problem import LeanMtsProblem, LeanMtsSolution
from .solver import LeanMtsSolver


class PulpMipSolver(LeanMtsSolver):
    """Solves LeanMtsProblem as the multiple-choice knapsack MIP described
    in the README, using PuLP's CBC backend."""

    def solve(self, problem: LeanMtsProblem) -> LeanMtsSolution:
        try:
            import pulp
        except ImportError as exc:
            raise RuntimeError(
                "pulp is not installed; run `pip install pulp` to use "
                "PulpMipSolver."
            ) from exc

        skus = problem.skus()
        n = len(skus)

        model = pulp.LpProblem("lean_mts", pulp.LpMinimize)

        # One binary variable y[i][k] per (SKU, card-count) choice.
        y = {}
        for i, sku in enumerate(skus):
            cap = sku.card_cap()
            for k in range(cap + 1):
                y[(i, k)] = pulp.LpVariable(f"y_{i}_{k}", cat="Binary")

        # Objective: sum of the chosen card-count's cost, over all SKUs.
        model += pulp.lpSum(
            sku.cost_for_cards(k) * y[(i, k)]
            for i, sku in enumerate(skus)
            for k in range(sku.card_cap() + 1)
        )

        # Exactly one card-count chosen per SKU.
        for i, sku in enumerate(skus):
            model += pulp.lpSum(y[(i, k)] for k in range(sku.card_cap() + 1)) == 1

        # Shared footprint budget: sum_i sum_k footprintPerCard_i * k * y_{i,k}
        # <= totalFootprintBudget.
        model += (
            pulp.lpSum(
                sku.footprint_per_card * k * y[(i, k)]
                for i, sku in enumerate(skus)
                for k in range(sku.card_cap() + 1)
            )
            <= problem.total_footprint_budget()
        )

        model.solve(pulp.PULP_CBC_CMD(msg=False))

        result = LeanMtsSolution()
        result.cards_allocated = [0] * n
        for i, sku in enumerate(skus):
            for k in range(sku.card_cap() + 1):
                if pulp.value(y[(i, k)]) > 0.5:
                    result.cards_allocated[i] = k
                    break

        problem.validate(result)
        return result

    def name(self) -> str:
        return "Pulp-MIP"
