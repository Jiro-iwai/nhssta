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
        // Step 1: Compute hash values for both elements
        // Hash based on the underlying shared_ptr addresses (pointer identity)
        auto h1 = std::hash<const void*>{}(p.first.get());
        auto h2 = std::hash<const void*>{}(p.second.get());
        
        // Step 2: Normalize hash values for symmetric hashing
        // Use min/max to ensure same hash for (a,b) and (b,a) after normalization
        // This works together with normalize() to provide consistent hashing
        auto lo = std::min(h1, h2);
        auto hi = std::max(h1, h2);
        
        // Step 3: Combine hashes using golden ratio multiplier
        // Golden ratio multiplier: 0x9e3779b9 = 2^32 / φ (where φ ≈ 1.618...)
        // Multiplying by an irrational number (via 2^32/φ) improves hash distribution
        // and reduces collisions. This is a well-established technique used in
        // many hash table implementations (e.g., Java HashMap, Python dict).
        // XOR combines lo and hi to mix both hash values into the final result.
        return lo ^ (hi * 0x9e3779b9);
    }
    
    // Hash collision handling:
    // When hash collisions occur (different keys produce the same hash value),
    // std::unordered_map automatically handles them using chaining:
    // 1. Keys with the same hash are stored in the same bucket (linked list)
    // 2. During lookup, std::unordered_map:
    //    a. Computes hash value to find the bucket
    //    b. Iterates through the chain comparing keys using operator==
    //    c. Returns the value when matching key is found
    // 3. Key equality is determined by std::pair's operator==, which compares
    //    both first and second elements using RandomVariable's operator==
    //    (pointer comparison via HandleBase)
    // 4. Performance: O(1) average case, O(n) worst case if all keys collide
    //    (very rare with current hash function due to golden ratio multiplier)
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
