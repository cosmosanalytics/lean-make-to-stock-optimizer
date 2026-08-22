#include <iomanip>
#include <iostream>
#include <vector>

#include "ExactDPSolver.h"
#include "GreedyMarginalCostSolver.h"
#include "LeanMtsProblem.h"

using namespace leanmts;

namespace {

void printSolution(const std::string& solverName, const LeanMtsProblem& problem,
                    const LeanMtsSolution& solution) {
    std::cout << "\n--- " << solverName << " ---\n";
    std::cout << "Feasible: " << (solution.feasible ? "yes" : "no") << "\n";
    std::cout << "Total annual cost: $" << std::fixed << std::setprecision(2)
               << solution.totalCost << "\n";
    std::cout << std::left << std::setw(14) << "SKU" << std::setw(8) << "Cards" << std::setw(10)
               << "Required" << "Footprint\n";
    const auto& skus = problem.skus();
    double usedFootprint = 0.0;
    for (std::size_t i = 0; i < skus.size(); ++i) {
        const int K = solution.cardsAllocated.empty() ? 0 : solution.cardsAllocated[i];
        usedFootprint += skus[i].footprintPerCard() * K;
        std::cout << std::left << std::setw(14) << skus[i].name() << std::setw(8) << K
                   << std::setw(10) << skus[i].requiredCards()
                   << std::setprecision(1) << (skus[i].footprintPerCard() * K) << "\n";
    }
    std::cout << "Footprint used: " << std::setprecision(1) << usedFootprint << " / "
               << problem.totalFootprintBudget() << "\n";
}

} // namespace

int main() {
    // A representative mixed SKU family on one shared pull board: fast
    // movers with tight service requirements, slow movers, and a couple of
    // low-value/high-shortage-tolerance items -- demand in units/day,
    // lead time in days, service-level z chosen per SKU criticality.
    std::vector<KanbanSku> skus = {
        KanbanSku(1, "Widget-A", /*containerSize=*/10, /*meanDemand=*/40, /*stdDev=*/8,
                   /*leadTime=*/2, /*holdingCost=*/15.0, /*shortageCost=*/250.0,
                   /*activationCost=*/80.0, /*footprint=*/1.0, /*z=*/1.65),
        KanbanSku(2, "Widget-B", 25, 60, 12, 3, 8.0, 180.0, 60.0, 1.0, 1.65),
        KanbanSku(3, "Bracket-C", 50, 90, 20, 1, 5.0, 150.0, 40.0, 1.0, 1.65),
        KanbanSku(4, "Fastener-D", 200, 500, 60, 2, 1.5, 90.0, 20.0, 2.0, 1.65),
        KanbanSku(5, "Housing-E", 15, 25, 6, 4, 25.0, 300.0, 120.0, 1.0, 1.65),
        KanbanSku(6, "Sensor-F", 5, 8, 3, 3, 45.0, 400.0, 150.0, 1.0, 1.65),
    };

    // The shared pull board has far less capacity than every SKU's ideal
    // card count would need -- forcing real trade-offs between SKUs.
    LeanMtsProblem problem(skus, /*totalFootprintBudget=*/12.0);

    std::cout << "Lean Make-to-Stock Optimizer -- Kanban Card Allocation\n";
    std::cout << skus.size() << " SKUs on a shared pull board, footprint budget = "
               << problem.totalFootprintBudget() << " slots\n";

    GreedyMarginalCostSolver greedy;
    printSolution(greedy.name(), problem, greedy.solve(problem));

    ExactDPSolver exact;
    printSolution(exact.name() + " (exact)", problem, exact.solve(problem));

    return 0;
}
