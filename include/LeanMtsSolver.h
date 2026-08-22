#pragma once

#include <string>

#include "LeanMtsProblem.h"

namespace leanmts {

// Strategy interface for card-allocation solvers, so main.cpp and the
// tests can swap heuristic/exact/production backends without caring which
// one they're driving.
class LeanMtsSolver {
public:
    virtual ~LeanMtsSolver() = default;
    virtual LeanMtsSolution solve(const LeanMtsProblem& problem) = 0;
    virtual std::string name() const = 0;
};

} // namespace leanmts
