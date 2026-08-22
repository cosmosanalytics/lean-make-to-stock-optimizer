#pragma once

#include <algorithm>
#include <cmath>
#include <string>

namespace leanmts {

// One SKU replenished through a kanban pull lane. Cards are the unit of
// control: each card authorizes production/withdrawal of one container of
// `containerSize` units, and the number of cards in circulation for a SKU
// is what actually determines how much buffer stock exists against demand
// during the replenishment lead time.
class KanbanSku {
public:
    KanbanSku(int id, std::string name, double containerSize, double demandMeanPerDay,
               double demandStdDevPerDay, double leadTimeDays, double holdingCostPerUnitPerYear,
               double shortageCostPerCardShort, double activationCost, double footprintPerCard,
               double serviceLevelZ)
        : id_(id), name_(std::move(name)), containerSize_(containerSize),
          demandMeanPerDay_(demandMeanPerDay), demandStdDevPerDay_(demandStdDevPerDay),
          leadTimeDays_(leadTimeDays), holdingCostPerUnitPerYear_(holdingCostPerUnitPerYear),
          shortageCostPerCardShort_(shortageCostPerCardShort), activationCost_(activationCost),
          footprintPerCard_(footprintPerCard), serviceLevelZ_(serviceLevelZ) {}

    int id() const { return id_; }
    const std::string& name() const { return name_; }
    double containerSize() const { return containerSize_; }
    double demandMeanPerDay() const { return demandMeanPerDay_; }
    double demandStdDevPerDay() const { return demandStdDevPerDay_; }
    double leadTimeDays() const { return leadTimeDays_; }
    double holdingCostPerUnitPerYear() const { return holdingCostPerUnitPerYear_; }
    double shortageCostPerCardShort() const { return shortageCostPerCardShort_; }
    double activationCost() const { return activationCost_; }
    double footprintPerCard() const { return footprintPerCard_; }
    double serviceLevelZ() const { return serviceLevelZ_; }

    // Classical kanban card-count formula: enough containers to cover mean
    // demand over the lead time, plus a safety-stock buffer sized off the
    // demand standard deviation via the target service level's z-score,
    // rounded up to a whole number of containers (you can't issue a
    // fractional card).
    int requiredCards() const {
        const double demandDuringLeadTime = demandMeanPerDay_ * leadTimeDays_;
        const double safetyBuffer =
            serviceLevelZ_ * demandStdDevPerDay_ * std::sqrt(leadTimeDays_);
        const double cards = (demandDuringLeadTime + safetyBuffer) / containerSize_;
        return static_cast<int>(std::ceil(cards - 1e-9));
    }

    // Practical upper bound on how many cards are ever worth allocating:
    // one past the number needed to fully cover demand. Provisioning
    // further than that never reduces cost (it only adds holding cost),
    // so it's excluded from every solver's search space up front.
    int cardCap() const { return requiredCards() + 1; }

    // Cost of operating this SKU's pull lane with exactly K cards on the
    // shared board, for one year.
    //
    // K == 0 means the SKU isn't stocked on the board at all: no
    // activation cost, no holding cost, but the entire required buffer is
    // "short" -- covered informally or not at all.
    //
    // K >= 1 means the lane is active: a fixed activation cost (printing
    // cards, staging a point-of-use bin) plus holding cost on every unit
    // carried across all K containers, plus a shortage penalty for any
    // cards still short of the SKU's ideal requiredCards().
    //
    // The fixed activationCost for K >= 1 is what makes this function
    // non-convex in K -- the first card is disproportionately expensive --
    // which is exactly what makes the allocation problem across SKUs
    // combinatorially interesting rather than a trivial per-SKU calculation.
    double costForCards(int K) const {
        const int required = requiredCards();
        if (K <= 0) {
            return shortageCostPerCardShort_ * required;
        }
        const double holding = holdingCostPerUnitPerYear_ * containerSize_ * K;
        const double shortage = shortageCostPerCardShort_ * std::max(0, required - K);
        return activationCost_ + holding + shortage;
    }

private:
    int id_;
    std::string name_;
    double containerSize_;
    double demandMeanPerDay_;
    double demandStdDevPerDay_;
    double leadTimeDays_;
    double holdingCostPerUnitPerYear_;
    double shortageCostPerCardShort_;
    double activationCost_;
    double footprintPerCard_;
    double serviceLevelZ_;
};

} // namespace leanmts
