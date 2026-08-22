#include <cmath>
#include <vector>

#include "ExactDPSolver.h"
#include "GreedyMarginalCostSolver.h"
#include "LeanMtsProblem.h"
#include "TestFramework.h"

using namespace leanmts;

namespace {
bool approxEqual(double a, double b, double eps = 1e-6) { return std::fabs(a - b) < eps; }

// Hand-verified 2-SKU setup (see README). Zero demand variability, so
// requiredCards() reduces to ceil(mean*leadTime/containerSize).
KanbanSku makeSkuA() {
    // requiredCards=3. Cost table: 30,23,14,5,6 (K=0..4).
    return KanbanSku(1, "SKU-A", /*containerSize=*/1.0, /*demandMeanPerDay=*/3.0,
                       /*demandStdDevPerDay=*/0.0, /*leadTimeDays=*/1.0,
                       /*holdingCostPerUnitPerYear=*/1.0, /*shortageCostPerCardShort=*/10.0,
                       /*activationCost=*/2.0, /*footprintPerCard=*/1.0, /*serviceLevelZ=*/1.65);
}

KanbanSku makeSkuB() {
    // requiredCards=2. Cost table: 20,12,3,4 (K=0..3).
    return KanbanSku(2, "SKU-B", 1.0, 2.0, 0.0, 1.0, 1.0, 10.0, 1.0, 1.0, 1.65);
}
} // namespace

TEST(CostModel_MatchesHandComputedTable) {
    KanbanSku a = makeSkuA();
    CHECK(a.requiredCards() == 3);
    CHECK(a.cardCap() == 4);
    CHECK(approxEqual(a.costForCards(0), 30.0));
    CHECK(approxEqual(a.costForCards(1), 23.0));
    CHECK(approxEqual(a.costForCards(2), 14.0));
    CHECK(approxEqual(a.costForCards(3), 5.0));
    CHECK(approxEqual(a.costForCards(4), 6.0));

    KanbanSku b = makeSkuB();
    CHECK(b.requiredCards() == 2);
    CHECK(b.cardCap() == 3);
    CHECK(approxEqual(b.costForCards(0), 20.0));
    CHECK(approxEqual(b.costForCards(1), 12.0));
    CHECK(approxEqual(b.costForCards(2), 3.0));
    CHECK(approxEqual(b.costForCards(3), 4.0));
}

TEST(ExactDP_AchievesKnownOptimum) {
    // Global min is 17, tied at (K_A=3,K_B=1)=5+12=17 and
    // (K_A=2,K_B=2)=14+3=17; only cost is asserted.
    std::vector<KanbanSku> skus = {makeSkuA(), makeSkuB()};
    LeanMtsProblem problem(skus, /*totalFootprintBudget=*/4.0);
    ExactDPSolver solver;
    LeanMtsSolution solution = solver.solve(problem);
    CHECK(solution.feasible);
    CHECK(solution.totalCost > 16.999 && solution.totalCost < 17.001);
}

TEST(ExactDP_NeverWorseThanGreedy) {
    std::vector<KanbanSku> skus = {makeSkuA(), makeSkuB()};
    LeanMtsProblem problem(skus, /*totalFootprintBudget=*/4.0);
    GreedyMarginalCostSolver greedy;
    ExactDPSolver exact;
    LeanMtsSolution greedySolution = greedy.solve(problem);
    LeanMtsSolution exactSolution = exact.solve(problem);
    CHECK(greedySolution.feasible);
    CHECK(exactSolution.feasible);
    CHECK(exactSolution.totalCost <= greedySolution.totalCost + 1e-9);
    // Greedy also happens to land on 17 here -- a coincidence of this
    // instance, not a guarantee; this test only checks "never worse".
}

TEST(Feasibility_DetectsBudgetViolation) {
    std::vector<KanbanSku> skus = {makeSkuA(), makeSkuB()};
    LeanMtsProblem problem(skus, /*totalFootprintBudget=*/2.0);
    LeanMtsSolution solution;
    solution.cardsAllocated = {3, 3}; // footprint used = 3+3 = 6 > budget 2
    problem.validate(solution);
    CHECK(!solution.feasible);
}

TEST(EdgeCase_ZeroSkus) {
    std::vector<KanbanSku> skus;
    LeanMtsProblem problem(skus, /*totalFootprintBudget=*/10.0);
    GreedyMarginalCostSolver greedy;
    ExactDPSolver exact;
    LeanMtsSolution greedySolution = greedy.solve(problem);
    LeanMtsSolution exactSolution = exact.solve(problem);
    CHECK(greedySolution.feasible);
    CHECK(greedySolution.totalCost == 0.0);
    CHECK(exactSolution.feasible);
    CHECK(exactSolution.totalCost == 0.0);
}

TEST(EdgeCase_ZeroBudget) {
    // No budget -> every SKU forced to K=0: cost = full shortage penalty.
    std::vector<KanbanSku> skus = {makeSkuA(), makeSkuB()};
    LeanMtsProblem problem(skus, /*totalFootprintBudget=*/0.0);
    ExactDPSolver exact;
    LeanMtsSolution solution = exact.solve(problem);
    CHECK(solution.feasible);
    CHECK(solution.cardsAllocated[0] == 0);
    CHECK(solution.cardsAllocated[1] == 0);
    CHECK(approxEqual(solution.totalCost, 50.0)); // A:10*3=30, B:10*2=20 -> 50
}

TEST(RequiredCards_AccountsForSafetyStock) {
    // mean=10, lead=4, containerSize=5, stdDev=3, z=1.65:
    // required = ceil((10*4 + 1.65*3*sqrt(4)) / 5) = ceil(49.9/5) = 10
    KanbanSku sku(9, "Variable-Demand-SKU", /*containerSize=*/5.0, /*demandMeanPerDay=*/10.0,
                   /*demandStdDevPerDay=*/3.0, /*leadTimeDays=*/4.0,
                   /*holdingCostPerUnitPerYear=*/2.0, /*shortageCostPerCardShort=*/50.0,
                   /*activationCost=*/10.0, /*footprintPerCard=*/1.0, /*serviceLevelZ=*/1.65);
    CHECK(sku.requiredCards() == 10);
    CHECK(sku.cardCap() == 11);

    // Zero demand variability needs no safety buffer at all.
    KanbanSku deterministic(10, "Deterministic-SKU", 5.0, 10.0, 0.0, 4.0, 2.0, 50.0, 10.0, 1.0,
                              1.65);
    CHECK(deterministic.requiredCards() == 8); // ceil(40/5) = 8
}

TEST(EdgeCase_BudgetExceedsTotalNeed_EveryoneGetsIdealAllocation) {
    // Budget generous enough that neither SKU is constrained by the
    // other, so each SKU's cost curve is minimized at K==requiredCards().
    std::vector<KanbanSku> skus = {makeSkuA(), makeSkuB()};
    LeanMtsProblem problem(skus, /*totalFootprintBudget=*/100.0);
    GreedyMarginalCostSolver greedy;
    ExactDPSolver exact;
    LeanMtsSolution greedySolution = greedy.solve(problem);
    LeanMtsSolution exactSolution = exact.solve(problem);
    CHECK(greedySolution.feasible);
    CHECK(exactSolution.feasible);
    CHECK(exactSolution.cardsAllocated[0] == skus[0].requiredCards());
    CHECK(exactSolution.cardsAllocated[1] == skus[1].requiredCards());
    CHECK(greedySolution.cardsAllocated[0] == skus[0].requiredCards());
    CHECK(greedySolution.cardsAllocated[1] == skus[1].requiredCards());
    CHECK(approxEqual(exactSolution.totalCost, greedySolution.totalCost));
}
