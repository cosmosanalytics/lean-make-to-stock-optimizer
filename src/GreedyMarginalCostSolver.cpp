#include "GreedyMarginalCostSolver.h"

#include <limits>

namespace leanmts {

LeanMtsSolution GreedyMarginalCostSolver::solve(const LeanMtsProblem& problem) {
    const auto& skus = problem.skus();

    LeanMtsSolution solution;
    solution.cardsAllocated.assign(skus.size(), 0);

    double remainingBudget = problem.totalFootprintBudget();

    // Repeatedly take the single best next +1-card move across all SKUs
    // until nothing left improves cost or fits the remaining budget.
    while (true) {
        int bestIdx = -1;
        double bestSaving = 1e-9; // require a strictly positive improvement

        for (std::size_t i = 0; i < skus.size(); ++i) {
            const KanbanSku& sku = skus[i];
            const int k = solution.cardsAllocated[i];
            if (k >= sku.cardCap()) continue;
            if (sku.footprintPerCard() > remainingBudget + 1e-9) continue;

            const double saving = sku.costForCards(k) - sku.costForCards(k + 1);
            if (saving > bestSaving) {
                bestSaving = saving;
                bestIdx = static_cast<int>(i);
            }
        }

        if (bestIdx < 0) break; // no move helps -- local optimum reached

        solution.cardsAllocated[bestIdx] += 1;
        remainingBudget -= skus[bestIdx].footprintPerCard();
    }

    problem.validate(solution);
    return solution;
}

} // namespace leanmts
