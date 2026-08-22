# Lean Make-to-Stock Optimizer (Python) — Kanban Card Allocation Under a Shared Pull-Board Budget

A Python 3 port of the C++ `lean_make_to_stock` portfolio project, modeling
a decision that comes up constantly in lean/pull manufacturing and S&OP
work: **how many kanban cards should each SKU get when the shared pull
board doesn't have room for everyone's ideal count?**

## The problem

In a lean pull system, SKUs are replenished through **kanban cards**: each
card authorizes production or withdrawal of exactly one container of fixed
size, and the number of cards in circulation for a SKU is what actually
sets how much buffer stock exists against demand during the replenishment
lead time. The classical formula for the number of cards a SKU *needs* to
hit a target service level is:

```
required_cards = ceil( (demand_mean_per_day * lead_time_days
                         + service_level_z * demand_std_dev_per_day * sqrt(lead_time_days))
                        / container_size )
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
cost(0) = shortage_cost_per_card_short * required_cards
```

A SKU with `K >= 1` cards has an active pull lane: a **fixed activation
cost** (printing cards, staging a point-of-use bin) plus holding cost on
every unit carried across all `K` containers, plus a shortage penalty for
any cards still short of `required_cards`:

```
cost(K) = activation_cost
          + holding_cost_per_unit_per_year * container_size * K
          + shortage_cost_per_card_short * max(0, required_cards - K)
```

The fixed `activation_cost` is what makes this interesting: it makes each
SKU's cost curve **non-convex** in `K` (the first card is disproportionately
expensive, since it's the only one that pays the activation fee). That
non-convexity is what a naive per-SKU calculation gets wrong, and what
makes allocating a shared budget across many SKUs combinatorially harder
than it looks.

### Knapsack formulation

```
minimize   sum_i cost_i(K_i)
subject to sum_i footprint_per_card_i * K_i <= total_footprint_budget
           0 <= K_i <= card_cap_i   (card_cap_i = required_cards_i + 1)
```

This is a **multiple-choice knapsack / separable resource-allocation
problem**: exactly one card-count choice must be made per SKU, each choice
has a cost and a footprint "weight," and the shared budget is the knapsack
capacity. The `card_cap_i = required_cards_i + 1` bound is a practical
upper limit — provisioning further than one card past full coverage never
reduces cost, it only adds holding cost.

## Design

- **`kanban_sku.py`** — `KanbanSku`, a frozen dataclass owning one SKU's
  demand/cost/service-level parameters, with `required_cards()`,
  `card_cap()`, and `cost_for_cards(k)` (the two-branch cost function
  above).
- **`problem.py`** — `LeanMtsProblem` owns the SKU list and shared
  footprint budget, and `validate()`s any candidate `LeanMtsSolution`
  (feasibility + total cost) independent of how that solution was
  produced.
- **`solver.py`** — `LeanMtsSolver`, a small ABC (`solve`, `name`) so
  `main.py` and the tests can swap backends freely.
- **`greedy_solver.py`** — `GreedyMarginalCostSolver`: a fast heuristic
  (steepest marginal-cost descent, one card at a time). Not guaranteed
  optimal — the fixed activation cost's non-convexity can trap it into a
  worse allocation than the global optimum.
- **`exact_solver.py`** — `ExactDPSolver`: a from-scratch bounded
  multiple-choice knapsack dynamic program over the shared footprint
  budget, with backpointers to recover the optimal per-SKU allocation.
  Handles the non-convex activation cost correctly where greedy can't.
- **`pulp_solver.py`** — `PulpMipSolver`: the same multiple-choice-knapsack
  model expressed as a MIP via PuLP + CBC (mirrors the C++ project's
  `CbcMipSolver.h`), one binary `y_{i,k}` per (SKU, card-count) pair.

The greedy and exact-DP solvers are **dependency-free** (Python stdlib
only) and are what the test suite exercises for its hand-verified
assertions. The PuLP/CBC solver is the **production-scale** path — it only
needs `pip install pulp`, and its test is skipped automatically if `pulp`
isn't installed.

## Build & run

```sh
pip install -r requirements.txt   # optional — only needed for pulp_solver

python3 -m unittest discover -s tests -v
python3 main.py
```

## Layout

```
leanmts/   KanbanSku, LeanMtsProblem/LeanMtsSolution, LeanMtsSolver ABC,
           GreedyMarginalCostSolver, ExactDPSolver, PulpMipSolver
tests/     test_lean_mts.py -- hand-verified cost table and knapsack
           optimum, feasibility checks, greedy-vs-exact comparison, edge
           cases (zero SKUs, zero budget, ample budget), and a
           pulp-vs-exact check (skipped if pulp isn't installed)
main.py    Demo: greedy + exact (+ pulp, if available) on a sample instance
```
