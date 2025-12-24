// -*- c++ -*-
// Author: IWAI Jiro

#include <algorithm>
#include <cassert>
#include <cmath>
#include <iostream>
#include <memory>
#include <nhssta/exception.hpp>

#include "add.hpp"
#include "covariance.hpp"
#include "covariance_context.hpp"
#include "expression.hpp"
#include "max.hpp"
#include "statistical_functions.hpp"
#include "statistics.hpp"
#include "sub.hpp"
#include "util_numerical.hpp"

namespace RandomVariable {

// ============================================================================
// Context-based API implementation (Phase 1 continued: Complete global state removal)
// ============================================================================

// Thread-local pointer to active CovarianceContext
// When set via ActiveContextGuard, covariance() functions will use this
// context instead of the default context
thread_local CovarianceContext* active_context_ = nullptr;

// Thread-local default CovarianceContext
// Each thread gets its own default context, ensuring thread safety
// This replaces the previous static global CovarianceMatrix
thread_local std::unique_ptr<CovarianceContext> default_context_ = nullptr;

void set_active_context(CovarianceContext* context) {
    active_context_ = context;
}

CovarianceContext* get_active_context() {
    return active_context_;
}

CovarianceContext* get_default_context() {
    // Lazy initialization: create default context on first access
    if (default_context_ == nullptr) {
        default_context_ = std::make_unique<CovarianceContext>();
    }
    return default_context_.get();
}

// Helper function: get active context or default context
// This is used internally by covariance() and cov_expr()
static CovarianceContext* get_context() {
    if (active_context_ != nullptr) {
        return active_context_;
    }
    return get_default_context();
}

// Backward compatibility: get_covariance_matrix() now returns
// the matrix from the current context (active or default)
CovarianceMatrix& get_covariance_matrix() {
    return get_context()->matrix();
}

bool CovarianceMatrixImpl::lookup(const RandomVariable& a, const RandomVariable& b,
                                double& cov) const {
    // Step 1: Cache lookup
    // Normalize key (ensure a <= b ordering) and search cache
    // This allows handling both (a, b) and (b, a) with a single search
    RowCol rowcol = normalize(a, b);
    auto i = cmat.find(rowcol);
    if (i != cmat.end()) {
        // Cache hit: return precomputed covariance value
        cov = i->second;
        return true;
    }

    // Step 2: Fallback handling (cache miss)
    // Check if both variables are NormalImpl (normal distribution) type
    // For NormalImpl types, we can compute immediately using statistical properties
    const auto* a_normal = dynamic_cast<const NormalImpl*>(a.get());
    const auto* b_normal = dynamic_cast<const NormalImpl*>(b.get());
    if (a_normal != nullptr && b_normal != nullptr) {
        // Both are normal distributions
        if (a == b) {
            // Same variable: use property Cov(X, X) = Var(X)
            cov = a->variance();
        } else {
            // Different variables: assume independent normal distributions (cov = 0)
            // If correlation exists, it will be computed recursively in the caller's
            // covariance() function
            cov = 0.0;
        }
        return true;
    }

    // Step 3: Fallback failed
    // For non-NormalImpl types (OpADD, OpMAX, OpMAX0, etc.),
    // delegate to more complex computation logic in the caller's covariance() function
    return false;
}

double covariance(const Normal& a, const Normal& b) {
    // Use active context or default context
    // All covariance logic is now in CovarianceContext
    return get_context()->covariance(a, b);
}

// ============================================================================
// Expression-based covariance (Phase C-5 of #167)
// ============================================================================

// Backward compatibility: clear_cov_expr_cache() now clears
// the expression cache from the current context (active or default)
void clear_cov_expr_cache() {
    get_context()->clear_expr_cache();
}

Expression cov_expr(const RandomVariable& a, const RandomVariable& b) {
    // Use active context or default context
    // All covariance logic is now in CovarianceContext
    return get_context()->cov_expr(a, b);
}

double covariance(const RandomVariable& a, const RandomVariable& b) {
    // Use active context or default context
    // All covariance logic is now in CovarianceContext
    return get_context()->covariance(a, b);
}
}  // namespace RandomVariable
