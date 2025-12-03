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
// Uses symmetric hashing for normalized keys (a <= b)
struct PairHash {
    template <class T1, class T2>
    std::size_t operator()(const std::pair<T1, T2>& p) const {
        // Hash based on the underlying shared_ptr addresses
        auto h1 = std::hash<const void*>{}(p.first.get());
        auto h2 = std::hash<const void*>{}(p.second.get());
        // Symmetric hash: use min/max to ensure same hash for (a,b) and (b,a)
        // after normalization
        auto lo = std::min(h1, h2);
        auto hi = std::max(h1, h2);
        // Use golden ratio multiplier for better hash distribution
        return lo ^ (hi * 0x9e3779b9);
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
        RowCol rowcol = normalize(a, b);
        cmat[rowcol] = cov;
    }

    // For testing/debugging
    void clear() { cmat.clear(); }
    [[nodiscard]] size_t size() const { return cmat.size(); }

   private:
    // Normalize key to ensure consistent ordering (a <= b by pointer address)
    // This allows (a, b) and (b, a) to use the same cache entry
    static RowCol normalize(const RandomVariable& a, const RandomVariable& b) {
        if (a.get() <= b.get()) {
            return {a, b};
        }
        return {b, a};
    }

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
Expression cov_expr(const RandomVariable& a, const RandomVariable& b);

// Clear the cov_expr cache (for testing)
void clear_cov_expr_cache();

CovarianceMatrix& get_covariance_matrix();
}  // namespace RandomVariable

#endif  // NH_COVARIANCE__H
