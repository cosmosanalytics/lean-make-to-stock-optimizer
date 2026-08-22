"""Demo script mirroring the C++ project's src/main.cpp."""

from leanmts import ExactDPSolver, GreedyMarginalCostSolver, KanbanSku, LeanMtsProblem


def print_solution(solver_name, problem, solution):
    print(f"\n--- {solver_name} ---")
    print(f"Feasible: {'yes' if solution.feasible else 'no'}")
    print(f"Total annual cost: ${solution.total_cost:.2f}")
    print(f"{'SKU':<14}{'Cards':<8}{'Required':<10}Footprint")
    skus = problem.skus()
    used_footprint = 0.0
    for i, sku in enumerate(skus):
        k = solution.cards_allocated[i] if solution.cards_allocated else 0
        used_footprint += sku.footprint_per_card * k
        print(f"{sku.name:<14}{k:<8}{sku.required_cards():<10}{sku.footprint_per_card * k:.1f}")
    print(f"Footprint used: {used_footprint:.1f} / {problem.total_footprint_budget()}")


def main():
    # A representative mixed SKU family on one shared pull board: fast
    # movers with tight service requirements, slow movers, and a couple of
    # low-value/high-shortage-tolerance items -- demand in units/day, lead
    # time in days, service-level z chosen per SKU criticality.
    skus = [
        KanbanSku(1, "Widget-A", container_size=10, demand_mean_per_day=40,
                  demand_std_dev_per_day=8, lead_time_days=2, holding_cost_per_unit_per_year=15.0,
                  shortage_cost_per_card_short=250.0, activation_cost=80.0,
                  footprint_per_card=1.0, service_level_z=1.65),
        KanbanSku(2, "Widget-B", 25, 60, 12, 3, 8.0, 180.0, 60.0, 1.0, 1.65),
        KanbanSku(3, "Bracket-C", 50, 90, 20, 1, 5.0, 150.0, 40.0, 1.0, 1.65),
        KanbanSku(4, "Fastener-D", 200, 500, 60, 2, 1.5, 90.0, 20.0, 2.0, 1.65),
        KanbanSku(5, "Housing-E", 15, 25, 6, 4, 25.0, 300.0, 120.0, 1.0, 1.65),
        KanbanSku(6, "Sensor-F", 5, 8, 3, 3, 45.0, 400.0, 150.0, 1.0, 1.65),
    ]

    # The shared pull board has far less capacity than every SKU's ideal
    # card count would need -- forcing real trade-offs between SKUs.
    problem = LeanMtsProblem(skus, total_footprint_budget=12.0)

    print("Lean Make-to-Stock Optimizer -- Kanban Card Allocation")
    print(f"{len(skus)} SKUs on a shared pull board, footprint budget = "
          f"{problem.total_footprint_budget()} slots")

    greedy = GreedyMarginalCostSolver()
    print_solution(greedy.name(), problem, greedy.solve(problem))

    exact = ExactDPSolver()
    print_solution(exact.name() + " (exact)", problem, exact.solve(problem))

    try:
        from leanmts.pulp_solver import PulpMipSolver

        pulp_solver = PulpMipSolver()
        print_solution(pulp_solver.name() + " (exact MIP)", problem, pulp_solver.solve(problem))
    except RuntimeError:
        print("\n--- Pulp-MIP ---\nSkipped: pulp is not installed (pip install pulp).")


if __name__ == "__main__":
    main()
