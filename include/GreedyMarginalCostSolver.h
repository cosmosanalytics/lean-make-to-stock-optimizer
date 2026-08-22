#pragma once

#include "LeanMtsSolver.h"

namespace leanmts {

// Fast heuristic: start every SKU at K=0 cards, then repeatedly grant one
// more card to whichever SKU currently offers the single largest cost
// reduction (steepest marginal-cost descent), subject to remaining
// footprint budget and each SKU's cardCap(). Stops when no remaining move
// improves total cost, or the budget is exhausted.
//
// NOT guaranteed optimal: each SKU's activation cost makes costForCards()
// non-convex in K (the first card is disproportionately expensive because
// it's the only one that pays the fixed activation cost), which can trap
// a purely local, one-step-at-a-time greedy into a worse allocation than
// the global optimum. ExactDPSolver is the from-scratch solver that
// handles that non-convexity correctly.
class GreedyMarginalCostSolver : public LeanMtsSolver {
public:
    LeanMtsSolution solve(const LeanMtsProblem& problem) override;
    std::string name() const override { return "Greedy-MarginalCost"; }
};

} // namespace leanmts
