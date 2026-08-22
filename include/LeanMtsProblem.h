#pragma once

#include <vector>

#include "KanbanSku.h"

namespace leanmts {

// A candidate card allocation produced by a solver: how many kanban cards
// each SKU gets, indexed identically to LeanMtsProblem::skus().
struct LeanMtsSolution {
    bool feasible = true;
    double totalCost = 0.0;
    std::vector<int> cardsAllocated; // one entry per SKU, same order/index as skus
};

// A shared-board kanban card allocation problem: decide how many cards
// K_i each SKU gets (0 <= K_i <= cardCap_i) to minimize total annual cost,
// subject to a shared footprint budget (board slots / floor space) that no
// single SKU controls on its own. This is a separable resource-allocation
// problem -- specifically a multiple-choice knapsack, since exactly one
// card-count choice must be made per SKU.
class LeanMtsProblem {
public:
    LeanMtsProblem(std::vector<KanbanSku> skus, double totalFootprintBudget);

    const std::vector<KanbanSku>& skus() const { return skus_; }
    double totalFootprintBudget() const { return totalFootprintBudget_; }

    // Recomputes feasibility and totalCost for a solution built by a
    // solver: the allocation vector must have one entry per SKU, every
    // K_i must fall within [0, cardCap_i], and total footprint used must
    // not exceed the shared budget. Cost is the sum of each SKU's
    // costForCards(K_i), independent of how the allocation was produced.
    void validate(LeanMtsSolution& solution) const;

private:
    std::vector<KanbanSku> skus_;
    double totalFootprintBudget_;
};

} // namespace leanmts
