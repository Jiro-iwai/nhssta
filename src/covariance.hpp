// -*- c++ -*-
// Author: IWAI Jiro

#ifndef NH_COVARIANCE__H
#define NH_COVARIANCE__H

#include <functional>
#include <memory>
#include <unordered_map>

#include "expression.hpp"
#include "handle.hpp"
#include "normal.hpp"
#include "random_variable.hpp"

namespace RandomVariable {

// Custom hash function for std::pair<RandomVariable, RandomVariable>
// Uses the underlying shared_ptr addresses for hashing
struct PairHash {
    template <class T1, class T2>
    std::size_t operator()(const std::pair<T1, T2>& p) const {
        // Hash based on the underlying shared_ptr addresses
        auto h1 = std::hash<const void*>{}(p.first.get());
        auto h2 = std::hash<const void*>{}(p.second.get());
        // Combine hashes
        return h1 ^ (h2 << 1);
    }
};

class CovarianceMatrixImpl {
   public:
    using RowCol = std::pair<RandomVariable, RandomVariable>;
    using Matrix = std::unordered_map<RowCol, double, PairHash>;

    CovarianceMatrixImpl() = default;
    virtual ~CovarianceMatrixImpl() = default;

    bool lookup(const RandomVariable& a, const RandomVariable& b, double& cov) const;

    void set(const RandomVariable& a, const RandomVariable& b, double cov) {
        RowCol rowcol(a, b);
        cmat[rowcol] = cov;
    }

    // For testing/debugging
    void clear() { cmat.clear(); }
    [[nodiscard]] size_t size() const { return cmat.size(); }

   private:
    Matrix cmat;
};

// CovarianceMatrixHandle: Handle for CovarianceMatrix using HandleBase template
class CovarianceMatrixHandle : public HandleBase<CovarianceMatrixImpl> {
   public:
    // Default constructor creates a new CovarianceMatrixImpl
    CovarianceMatrixHandle()
        : HandleBase(std::make_shared<CovarianceMatrixImpl>()) {}

    // Inherit other constructors from HandleBase
    using HandleBase<CovarianceMatrixImpl>::HandleBase;
};

// Type alias: CovarianceMatrix is a Handle (thin wrapper around std::shared_ptr)
using CovarianceMatrix = CovarianceMatrixHandle;

double covariance(const Normal& a, const Normal& b);
double covariance(const RandomVariable& a, const RandomVariable& b);

// Expression-based covariance for automatic differentiation (Phase C-5 of #167)
// Returns an Expression representing Cov(a, b) that can be differentiated
Expression cov_expr(const RandomVariable& a, const RandomVariable& b, int depth = 0);

// Clear the cov_expr cache (for testing)
void clear_cov_expr_cache();

// Profiling statistics for cov_expr
void reset_cov_expr_stats();
void print_cov_expr_stats();

CovarianceMatrix& get_covariance_matrix();
}  // namespace RandomVariable

#endif  // NH_COVARIANCE__H
