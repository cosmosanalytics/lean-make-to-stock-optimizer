#pragma once

// CbcMipSolver -- production-scale path using COIN-OR CBC's C++ API.
// ExactDPSolver.h is the zero-dependency exact solver used by default;
// this documents how the same multiple-choice-knapsack model maps onto
// CBC directly (the general path once footprints stop being integral, or
// instances get large enough that a from-scratch DP isn't the right
// engineering choice anymore).
//
// Compiled only when LEANMTS_USE_CBC is defined (CMakeLists.txt's
// USE_CBC option), since it needs the COIN-OR CBC dev libraries:
//   sudo apt-get install coinor-libcbc-dev coinor-libclp-dev \
//                         coinor-libosi-dev coinor-libcoinutils-dev
//   cmake -DUSE_CBC=ON -B build && cmake --build build

#ifdef LEANMTS_USE_CBC

#include <vector>

#include <CbcModel.hpp>
#include <CoinPackedMatrix.hpp>
#include <OsiClpSolverInterface.hpp>

#include "LeanMtsSolver.h"

namespace leanmts {

// Solves LeanMtsProblem as the multiple-choice knapsack MIP described in
// the README: one binary y_{i,k} per (SKU, card-count) pair -- y_{i,k} = 1
// iff SKU i is allocated exactly k cards -- with exactly one k chosen per
// SKU and the shared footprint budget enforced as a single knapsack row.
class CbcMipSolver : public LeanMtsSolver {
public:
    LeanMtsSolution solve(const LeanMtsProblem& problem) override {
        const auto& skus = problem.skus();
        const int n = static_cast<int>(skus.size());

        // Flatten every (SKU index, card count) pair into one variable list.
        std::vector<int> skuOfVar;
        std::vector<int> cardsOfVar;
        std::vector<int> firstVarForSku(n, -1);
        for (int i = 0; i < n; ++i) {
            firstVarForSku[i] = static_cast<int>(skuOfVar.size());
            const int cap = skus[i].cardCap();
            for (int k = 0; k <= cap; ++k) {
                skuOfVar.push_back(i);
                cardsOfVar.push_back(k);
            }
        }
        const int numVars = static_cast<int>(skuOfVar.size());

        OsiClpSolverInterface solver;

        std::vector<double> objective(numVars, 0.0);
        for (int v = 0; v < numVars; ++v) {
            objective[v] = skus[skuOfVar[v]].costForCards(cardsOfVar[v]);
        }

        std::vector<double> colLower(numVars, 0.0);
        std::vector<double> colUpper(numVars, 1.0);

        CoinPackedMatrix matrix(false, 0, 0);
        matrix.setDimensions(0, numVars);

        std::vector<double> rowLower;
        std::vector<double> rowUpper;

        // sum_k y_{i,k} = 1 for every SKU i -- exactly one card count chosen.
        for (int i = 0; i < n; ++i) {
            CoinPackedVector row;
            const int cap = skus[i].cardCap();
            const int base = firstVarForSku[i];
            for (int k = 0; k <= cap; ++k) row.insert(base + k, 1.0);
            matrix.appendRow(row);
            rowLower.push_back(1.0);
            rowUpper.push_back(1.0);
        }

        // sum_i sum_k footprintPerCard_i * k * y_{i,k} <= totalFootprintBudget.
        {
            CoinPackedVector row;
            for (int v = 0; v < numVars; ++v) {
                const double coeff = skus[skuOfVar[v]].footprintPerCard() * cardsOfVar[v];
                if (coeff != 0.0) row.insert(v, coeff);
            }
            matrix.appendRow(row);
            rowLower.push_back(-COIN_DBL_MAX);
            rowUpper.push_back(problem.totalFootprintBudget());
        }

        solver.loadProblem(matrix, colLower.data(), colUpper.data(),
                            objective.data(), rowLower.data(), rowUpper.data());
        for (int v = 0; v < numVars; ++v) solver.setInteger(v);

        CbcModel model(solver);
        model.setLogLevel(0);
        model.branchAndBound();

        LeanMtsSolution result;
        result.cardsAllocated.assign(n, 0);
        const double* sol = model.solver()->getColSolution();
        for (int v = 0; v < numVars; ++v) {
            if (sol[v] > 0.5) {
                result.cardsAllocated[skuOfVar[v]] = cardsOfVar[v];
            }
        }
        problem.validate(result);
        return result;
    }

    std::string name() const override { return "Cbc-MIP"; }
};

} // namespace leanmts

#endif // LEANMTS_USE_CBC
