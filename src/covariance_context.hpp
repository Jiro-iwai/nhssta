// -*- c++ -*-
// Author: IWAI Jiro
//
// CovarianceContext: Non-global covariance computation context
//
// This class eliminates the global covariance_matrix state, enabling:
// 1. Better testability - each test can have isolated context
// 2. Parallel processing - multiple contexts can run independently
// 3. Explicit dependency management - no hidden global state
// 4. Easier mocking and unit testing

#ifndef NH_COVARIANCE_CONTEXT__H
#define NH_COVARIANCE_CONTEXT__H

#include <memory>
#include <unordered_map>

#include "covariance.hpp"
#include "expression.hpp"
#include "normal.hpp"
#include "random_variable.hpp"

namespace RandomVariable {

// Forward declarations
class CovarianceContextImpl;

/**
 * CovarianceContext: Manages covariance computation with isolated state
 *
 * This class replaces the global covariance_matrix with an explicit context
 * that can be passed around and managed explicitly.
 *
 * Key benefits:
 * - Test isolation: Each test can create its own context
 * - Parallel safety: Multiple contexts don't interfere with each other
 * - Clear dependencies: No hidden global state
 * - Memory management: Context lifecycle is explicit
 *
 * Usage:
 *   auto context = std::make_shared<CovarianceContext>();
 *   double cov = context->covariance(a, b);
 */
class CovarianceContext {
   public:
    CovarianceContext();
    ~CovarianceContext();

    // Disable copy but allow move
    CovarianceContext(const CovarianceContext&) = delete;
    CovarianceContext& operator=(const CovarianceContext&) = delete;
    CovarianceContext(CovarianceContext&&) = default;
    CovarianceContext& operator=(CovarianceContext&&) = default;

    // ========================================================================
    // Covariance computation methods
    // ========================================================================

    /**
     * Compute covariance between two Normal variables
     * @param a First Normal variable
     * @param b Second Normal variable
     * @return Covariance value
     */
    double covariance(const Normal& a, const Normal& b);

    /**
     * Compute covariance between two RandomVariables
     * @param a First RandomVariable
     * @param b Second RandomVariable
     * @return Covariance value
     */
    double covariance(const RandomVariable& a, const RandomVariable& b);

    /**
     * Compute covariance as Expression (for automatic differentiation)
     * @param a First RandomVariable
     * @param b Second RandomVariable
     * @return Expression representing Cov(a, b)
     */
    Expression cov_expr(const RandomVariable& a, const RandomVariable& b);

    // ========================================================================
    // Cache management
    // ========================================================================

    /**
     * Clear the double-based covariance cache
     */
    void clear();

    /**
     * Get size of double-based covariance cache
     * @return Number of cached covariance values
     */
    [[nodiscard]] size_t cache_size() const;

    /**
     * Clear the Expression-based covariance cache
     */
    void clear_expr_cache();

    /**
     * Get size of Expression-based covariance cache
     * @return Number of cached expression values
     */
    [[nodiscard]] size_t expr_cache_size() const;

    /**
     * Clear all caches (both double and Expression)
     */
    void clear_all_caches();

    // ========================================================================
    // Access to underlying matrix (for migration compatibility)
    // ========================================================================

    /**
     * Get the underlying CovarianceMatrix
     * Used during migration to maintain compatibility with existing code
     * @return Reference to the covariance matrix
     */
    CovarianceMatrix& matrix();

   private:
    std::unique_ptr<CovarianceContextImpl> impl_;
};

}  // namespace RandomVariable

#endif  // NH_COVARIANCE_CONTEXT__H
