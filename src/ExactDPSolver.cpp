#include "ExactDPSolver.h"

#include <cmath>
#include <limits>

namespace leanmts {

namespace {

constexpr double kInfCost = std::numeric_limits<double>::max();

// Rounds a real-valued footprint to the nearest whole budget unit. Board
// slots / floor positions are inherently discrete, so this is a faithful
// (not lossy, for the integral inputs this project expects) mapping onto
// the DP's integer state space.
int footprintUnits(double footprintPerCard, int cards) {
    return static_cast<int>(std::llround(footprintPerCard * cards));
}

} // namespace

LeanMtsSolution ExactDPSolver::solve(const LeanMtsProblem& problem) {
    const auto& skus = problem.skus();
    const int n = static_cast<int>(skus.size());

    LeanMtsSolution result;
    if (n == 0) {
        result.feasible = true;
        result.totalCost = 0.0;
        return result;
    }

    const int budget =
        std::max(0, static_cast<int>(std::llround(problem.totalFootprintBudget())));

    // dp[usedBudget] = minimum total cost of the SKUs decided so far,
    // consuming exactly usedBudget footprint units.
    std::vector<double> dp(budget + 1, kInfCost);
    dp[0] = 0.0;

    // choice[i][usedBudgetAfterSku_i] = the card count chosen for SKU i
    // that achieves dp's value at that state, so the allocation can be
    // reconstructed by walking backwards once the DP is complete.
    std::vector<std::vector<int>> choice(n, std::vector<int>(budget + 1, -1));

    for (int i = 0; i < n; ++i) {
        const KanbanSku& sku = skus[i];
        const int cap = sku.cardCap();

        std::vector<double> nextDp(budget + 1, kInfCost);
        for (int used = 0; used <= budget; ++used) {
            if (dp[used] >= kInfCost) continue;
            for (int k = 0; k <= cap; ++k) {
                const int fp = footprintUnits(sku.footprintPerCard(), k);
                const int newUsed = used + fp;
                if (newUsed > budget) break; // fp is non-decreasing in k
                const double newCost = dp[used] + sku.costForCards(k);
                if (newCost < nextDp[newUsed] - 1e-12) {
                    nextDp[newUsed] = newCost;
                    choice[i][newUsed] = k;
                }
            }
        }
        dp = std::move(nextDp);
    }

    // Best total cost may leave some budget unused -- scan every state.
    int bestState = 0;
    double bestCost = dp[0];
    for (int used = 1; used <= budget; ++used) {
        if (dp[used] < bestCost) {
            bestCost = dp[used];
            bestState = used;
        }
    }

    result.cardsAllocated.assign(n, 0);
    if (bestCost < kInfCost) {
        int state = bestState;
        for (int i = n - 1; i >= 0; --i) {
            const int k = choice[i][state];
            result.cardsAllocated[i] = k;
            state -= footprintUnits(skus[i].footprintPerCard(), k);
        }
    }

    problem.validate(result);
    return result;
}

} // namespace leanmts
