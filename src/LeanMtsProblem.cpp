#include "LeanMtsProblem.h"

namespace leanmts {

LeanMtsProblem::LeanMtsProblem(std::vector<KanbanSku> skus, double totalFootprintBudget)
    : skus_(std::move(skus)), totalFootprintBudget_(totalFootprintBudget) {}

void LeanMtsProblem::validate(LeanMtsSolution& solution) const {
    bool feasible = true;

    if (solution.cardsAllocated.size() != skus_.size()) {
        solution.feasible = false;
        solution.totalCost = 0.0;
        return;
    }

    double usedFootprint = 0.0;
    double cost = 0.0;
    for (std::size_t i = 0; i < skus_.size(); ++i) {
        const KanbanSku& sku = skus_[i];
        const int K = solution.cardsAllocated[i];
        if (K < 0 || K > sku.cardCap()) {
            feasible = false;
        }
        usedFootprint += sku.footprintPerCard() * K;
        cost += sku.costForCards(K);
    }

    if (usedFootprint > totalFootprintBudget_ + 1e-9) {
        feasible = false;
    }

    solution.feasible = feasible;
    solution.totalCost = feasible ? cost : 0.0;
}

} // namespace leanmts
