#pragma once

#include "LeanMtsSolver.h"

namespace leanmts {

// Exact, dependency-free solver: a standard bounded multiple-choice
// knapsack dynamic program over the shared footprint budget.
//
// For each SKU i, costForCards(k) is precomputed for every k = 0..cardCap_i
// (its "choices"). SKUs are processed one at a time; dp[usedBudget] tracks
// the minimum total cost achievable after deciding the first several SKUs
// while consuming exactly `usedBudget` footprint units. Because the DP
// tries every k for every SKU rather than assuming the cost curve is
// convex, it handles the fixed-activation-cost non-convexity correctly --
// unlike GreedyMarginalCostSolver, which can get trapped by it.
//
// The budget is treated as an integral number of footprint units (whole
// board slots / floor positions), which is how kanban board capacity is
// denominated in practice; totalFootprintBudget and each SKU's
// footprintPerCard are rounded to the nearest whole unit for DP indexing.
//
// Runtime is O(numSkus * budget * maxCardCap) -- pseudo-polynomial, like
// any knapsack DP, but exact and easily fast enough for the small/medium
// instances a portfolio demo or regression test uses. CbcMipSolver.h is
// the path for production scale / general (non-integral) footprints.
class ExactDPSolver : public LeanMtsSolver {
public:
    LeanMtsSolution solve(const LeanMtsProblem& problem) override;
    std::string name() const override { return "ExactDP-Optimal"; }
};

} // namespace leanmts
