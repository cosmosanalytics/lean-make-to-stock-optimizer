# Lean Make-to-Stock Optimizer (C++) — Kanban Card Allocation Under a Shared Pull-Board Budget

A C++17 portfolio project modeling a decision that comes up constantly in
lean/pull manufacturing and S&OP work: **how many kanban cards should each
SKU get when the shared pull board doesn't have room for everyone's ideal
count?** It's a companion to
[`network-optimization-2`](https://github.com/cosmosanalytics/network-optimization-2)
and [`network-optimization-3`](https://github.com/cosmosanalytics/network-optimization-3)
and follows the same structure: a solver-agnostic problem definition, a
`Strategy` interface, a fast heuristic, a dependency-free exact solver, and
(behind a build flag) a production path through COIN-OR CBC.

## The problem

In a lean pull system, SKUs are replenished through **kanban cards**: each
card authorizes production or withdrawal of exactly one container of fixed
size, and the number of cards in circulation for a SKU is what actually
sets how much buffer stock exists against demand during the replenishment
lead time. The classical formula for the number of cards a SKU *needs* to
hit a target service level is:

```
requiredCards = ceil( (demandMeanPerDay * leadTimeDays
                        + serviceLevelZ * demandStdDevPerDay * sqrt(leadTimeDays))
                       / containerSize )
```

— mean demand over the lead time, plus a safety buffer sized off demand
variability and the target service level's z-score, rounded up to a whole
number of containers.

The catch: the shop floor's pull board (or the physical storage area kanban
cards represent) has **limited shared capacity** — board slots, floor
positions, staging locations. Not every SKU can get its ideal card count.
This project decides how many cards `K_i` to actually allocate to each SKU,
to minimize total annual cost subject to that shared budget.

### Cost model

A SKU with `K = 0` cards isn't on the pull board at all — fully
unbuffered, so it pays only a shortage penalty for its entire requirement:

```
cost(0) = shortageCostPerCardShort * requiredCards
```

A SKU with `K >= 1` cards has an active pull lane: a **fixed activation
cost** (printing cards, staging a point-of-use bin) plus holding cost on
every unit carried across all `K` containers, plus a shortage penalty for
any cards still short of `requiredCards`:

```
cost(K) = activationCost
          + holdingCostPerUnitPerYear * containerSize * K
          + shortageCostPerCardShort * max(0, requiredCards - K)
```

The fixed `activationCost` is what makes this interesting: it makes each
SKU's cost curve **non-convex** in `K` (the first card is disproportionately
expensive, since it's the only one that pays the activation fee). That
non-convexity is exactly what a naive per-SKU calculation gets wrong, and
what makes allocating a shared budget across many SKUs combinatorially
harder than it looks.

### Knapsack formulation

Subject to the shared constraint

```
sum_i footprintPerCard_i * K_i <= totalFootprintBudget
```

the problem is:

```
minimize   sum_i cost_i(K_i)
subject to sum_i footprintPerCard_i * K_i <= totalFootprintBudget
           0 <= K_i <= cardCap_i   (cardCap_i = requiredCards_i + 1)
```

This is a **multiple-choice knapsack / separable resource-allocation
problem**: exactly one card-count choice must be made per SKU, each choice
has a cost and a footprint "weight," and the shared budget is the knapsack
capacity. The `cardCap_i = requiredCards_i + 1` bound is a practical upper
limit — provisioning further than one card past full coverage never
reduces cost, it only adds holding cost, so every solver excludes it from
the search space up front.

## Design

- **`KanbanSku`** owns one SKU's demand/cost/service-level parameters and
  computes `requiredCards()`, `cardCap()`, and `costForCards(K)` — the
  two-branch cost function above.
- **`LeanMtsProblem`** owns the SKU list and the shared footprint budget,
  and validates any candidate `LeanMtsSolution` (feasibility + total cost)
  independent of how that solution was produced.
- **`LeanMtsSolver`** is a small `Strategy` interface (`solve`, `name`) so
  `main.cpp` and the tests can swap backends freely.
- **`GreedyMarginalCostSolver`** — a fast heuristic: start every SKU at
  `K=0`, then repeatedly grant one more card to whichever SKU currently
  offers the single largest cost reduction (steepest marginal-cost
  descent), until nothing left improves cost or the budget runs out. It is
  **not** guaranteed optimal — the fixed activation cost's non-convexity
  can trap a purely local, one-card-at-a-time greedy into a worse
  allocation than the global optimum.
- **`ExactDPSolver`** — a from-scratch, zero-dependency exact solver: a
  standard bounded multiple-choice knapsack dynamic program over the
  shared footprint budget. Every SKU's full cost table (`costForCards(k)`
  for `k = 0..cardCap`) is precomputed; SKUs are processed one at a time
  into a `dp[usedBudget] = min cost so far` array, and the optimal
  per-SKU allocation is recovered via backpointers. Because it evaluates
  every `k` explicitly rather than assuming convexity, it handles the
  non-convex activation cost correctly where greedy can't.
- **`CbcMipSolver`** (behind `LEANMTS_USE_CBC`) documents the same model
  expressed directly against COIN-OR CBC's C++ API (`CbcModel`,
  `OsiClpSolverInterface`) as one binary `y_{i,k}` per (SKU, card-count)
  pair — the production-scale path.

## Build & run

```sh
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build

./build/leanmts_demo     # runs greedy + exact solvers on a sample instance
./build/leanmts_tests    # unit tests
```

To build with the real CBC backend instead of just documenting it:

```sh
sudo apt-get install coinor-libcbc-dev coinor-libclp-dev \
                      coinor-libosi-dev coinor-libcoinutils-dev
cmake -DUSE_CBC=ON -B build
cmake --build build
```

## Layout

```
include/   KanbanSku, LeanMtsProblem, LeanMtsSolver interface,
           GreedyMarginalCostSolver, ExactDPSolver, CbcMipSolver
src/       Implementations of the above (LeanMtsProblem.cpp,
           GreedyMarginalCostSolver.cpp, ExactDPSolver.cpp) + main.cpp
tests/     Dependency-free unit test harness (TestFramework.h) and
           LeanMtsTests.cpp — a hand-verified cost table and knapsack
           optimum, feasibility checks, greedy-vs-exact comparison, and
           edge cases (zero SKUs, zero budget, ample budget)
```
