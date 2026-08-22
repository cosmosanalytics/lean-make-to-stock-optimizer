"""Port of tests/LeanMtsTests.cpp -- every case, same hand-verified numbers."""

import unittest

from leanmts import (
    ExactDPSolver,
    GreedyMarginalCostSolver,
    KanbanSku,
    LeanMtsProblem,
    LeanMtsSolution,
)

try:
    import pulp  # noqa: F401

    PULP_AVAILABLE = True
except ImportError:
    PULP_AVAILABLE = False


def make_sku_a() -> KanbanSku:
    # requiredCards=3. Cost table: 30,23,14,5,6 (K=0..4).
    return KanbanSku(1, "SKU-A", 1.0, 3.0, 0.0, 1.0, 1.0, 10.0, 2.0, 1.0, 1.65)


def make_sku_b() -> KanbanSku:
    # requiredCards=2. Cost table: 20,12,3,4 (K=0..3).
    return KanbanSku(2, "SKU-B", 1.0, 2.0, 0.0, 1.0, 1.0, 10.0, 1.0, 1.0, 1.65)


class LeanMtsTests(unittest.TestCase):
    def test_cost_model_matches_hand_computed_table(self):
        a = make_sku_a()
        self.assertEqual(a.required_cards(), 3)
        self.assertEqual(a.card_cap(), 4)
        self.assertAlmostEqual(a.cost_for_cards(0), 30.0)
        self.assertAlmostEqual(a.cost_for_cards(1), 23.0)
        self.assertAlmostEqual(a.cost_for_cards(2), 14.0)
        self.assertAlmostEqual(a.cost_for_cards(3), 5.0)
        self.assertAlmostEqual(a.cost_for_cards(4), 6.0)

        b = make_sku_b()
        self.assertEqual(b.required_cards(), 2)
        self.assertEqual(b.card_cap(), 3)
        self.assertAlmostEqual(b.cost_for_cards(0), 20.0)
        self.assertAlmostEqual(b.cost_for_cards(1), 12.0)
        self.assertAlmostEqual(b.cost_for_cards(2), 3.0)
        self.assertAlmostEqual(b.cost_for_cards(3), 4.0)

    def test_exact_dp_achieves_known_optimum(self):
        # Global min is 17, tied at (K_A=3,K_B=1)=5+12=17 and
        # (K_A=2,K_B=2)=14+3=17; only cost is asserted.
        skus = [make_sku_a(), make_sku_b()]
        problem = LeanMtsProblem(skus, total_footprint_budget=4.0)
        solution = ExactDPSolver().solve(problem)
        self.assertTrue(solution.feasible)
        self.assertTrue(16.999 < solution.total_cost < 17.001)

    def test_exact_dp_never_worse_than_greedy(self):
        skus = [make_sku_a(), make_sku_b()]
        problem = LeanMtsProblem(skus, total_footprint_budget=4.0)
        greedy_solution = GreedyMarginalCostSolver().solve(problem)
        exact_solution = ExactDPSolver().solve(problem)
        self.assertTrue(greedy_solution.feasible)
        self.assertTrue(exact_solution.feasible)
        self.assertLessEqual(exact_solution.total_cost, greedy_solution.total_cost + 1e-9)
        # Greedy also happens to land on 17 here -- a coincidence of this
        # instance, not a guarantee; this test only checks "never worse".

    def test_feasibility_detects_budget_violation(self):
        skus = [make_sku_a(), make_sku_b()]
        problem = LeanMtsProblem(skus, total_footprint_budget=2.0)
        solution = LeanMtsSolution()
        solution.cards_allocated = [3, 3]  # footprint used = 3+3 = 6 > budget 2
        problem.validate(solution)
        self.assertFalse(solution.feasible)

    def test_edge_case_zero_skus(self):
        problem = LeanMtsProblem([], total_footprint_budget=10.0)
        greedy_solution = GreedyMarginalCostSolver().solve(problem)
        exact_solution = ExactDPSolver().solve(problem)
        self.assertTrue(greedy_solution.feasible)
        self.assertEqual(greedy_solution.total_cost, 0.0)
        self.assertTrue(exact_solution.feasible)
        self.assertEqual(exact_solution.total_cost, 0.0)

    def test_edge_case_zero_budget(self):
        # No budget -> every SKU forced to K=0: cost = full shortage penalty.
        skus = [make_sku_a(), make_sku_b()]
        problem = LeanMtsProblem(skus, total_footprint_budget=0.0)
        solution = ExactDPSolver().solve(problem)
        self.assertTrue(solution.feasible)
        self.assertEqual(solution.cards_allocated[0], 0)
        self.assertEqual(solution.cards_allocated[1], 0)
        self.assertAlmostEqual(solution.total_cost, 50.0)  # A:10*3=30, B:10*2=20 -> 50

    def test_required_cards_accounts_for_safety_stock(self):
        # mean=10, lead=4, containerSize=5, stdDev=3, z=1.65:
        # required = ceil((10*4 + 1.65*3*sqrt(4)) / 5) = ceil(49.9/5) = 10
        sku = KanbanSku(9, "Variable-Demand-SKU", 5.0, 10.0, 3.0, 4.0, 2.0, 50.0, 10.0, 1.0, 1.65)
        self.assertEqual(sku.required_cards(), 10)
        self.assertEqual(sku.card_cap(), 11)

        # Zero demand variability needs no safety buffer at all.
        deterministic = KanbanSku(10, "Deterministic-SKU", 5.0, 10.0, 0.0, 4.0, 2.0, 50.0, 10.0, 1.0, 1.65)
        self.assertEqual(deterministic.required_cards(), 8)  # ceil(40/5) = 8

    def test_edge_case_budget_exceeds_total_need_everyone_gets_ideal_allocation(self):
        # Budget generous enough that neither SKU is constrained by the
        # other, so each SKU's cost curve is minimized at K==requiredCards().
        skus = [make_sku_a(), make_sku_b()]
        problem = LeanMtsProblem(skus, total_footprint_budget=100.0)
        greedy_solution = GreedyMarginalCostSolver().solve(problem)
        exact_solution = ExactDPSolver().solve(problem)
        self.assertTrue(greedy_solution.feasible)
        self.assertTrue(exact_solution.feasible)
        self.assertEqual(exact_solution.cards_allocated[0], skus[0].required_cards())
        self.assertEqual(exact_solution.cards_allocated[1], skus[1].required_cards())
        self.assertEqual(greedy_solution.cards_allocated[0], skus[0].required_cards())
        self.assertEqual(greedy_solution.cards_allocated[1], skus[1].required_cards())
        self.assertAlmostEqual(exact_solution.total_cost, greedy_solution.total_cost)

    @unittest.skipUnless(PULP_AVAILABLE, "pulp not installed")
    def test_pulp_solver_matches_exact_dp(self):
        from leanmts.pulp_solver import PulpMipSolver

        skus = [make_sku_a(), make_sku_b()]
        problem = LeanMtsProblem(skus, total_footprint_budget=4.0)
        exact_solution = ExactDPSolver().solve(problem)
        pulp_solution = PulpMipSolver().solve(problem)
        self.assertTrue(pulp_solution.feasible)
        self.assertAlmostEqual(pulp_solution.total_cost, exact_solution.total_cost, places=6)


if __name__ == "__main__":
    unittest.main()
