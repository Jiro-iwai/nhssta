/**
 * @file test_expression_helpers.cpp
 * @brief Implementation of test-only Expression helper functions
 */

#include "test_expression_helpers.hpp"
#include "expression.hpp"
#include "statistical_functions.hpp"
using namespace RandomVariable;
#include <unordered_map>
#include <tuple>
#include <cmath>
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// Cache for phi2_expr_test (pointer-based)
using Phi2PDFCacheKey = std::tuple<ExpressionImpl*, ExpressionImpl*, ExpressionImpl*>;

struct Phi2PDFCacheKeyHash {
    std::size_t operator()(const Phi2PDFCacheKey& key) const {
        std::size_t h1 = std::hash<ExpressionImpl*>{}(std::get<0>(key));
        std::size_t h2 = std::hash<ExpressionImpl*>{}(std::get<1>(key));
        std::size_t h3 = std::hash<ExpressionImpl*>{}(std::get<2>(key));
        return h1 ^ (h2 << 1) ^ (h3 << 2);
    }
};

static std::unordered_map<Phi2PDFCacheKey, Expression, Phi2PDFCacheKeyHash> phi2_pdf_expr_cache;

// Cache for Phi2_expr_test (pointer-based)
using Phi2CacheKey = std::tuple<ExpressionImpl*, ExpressionImpl*, ExpressionImpl*>;

struct Phi2CacheKeyHash {
    std::size_t operator()(const Phi2CacheKey& key) const {
        std::size_t h1 = std::hash<ExpressionImpl*>{}(std::get<0>(key));
        std::size_t h2 = std::hash<ExpressionImpl*>{}(std::get<1>(key));
        std::size_t h3 = std::hash<ExpressionImpl*>{}(std::get<2>(key));
        return h1 ^ (h2 << 1) ^ (h3 << 2);
    }
};

static std::unordered_map<Phi2CacheKey, Expression, Phi2CacheKeyHash> phi2_expr_cache;

// Bivariate standard normal CDF Φ₂(h, k; ρ)
// NOTE: This function is for testing purposes only.
// In production code, Phi2_expr() uses Native type custom function.
Expression Phi2_expr_test(const Expression& h, const Expression& k, const Expression& rho) {
    // Φ₂(h, k; ρ) - use production Phi2_expr which now uses Native custom function
    
    // Check cache first
    auto key = std::make_tuple(h.get(), k.get(), rho.get());
    auto it = phi2_expr_cache.find(key);
    if (it != phi2_expr_cache.end()) {
        return it->second;
    }
    
    // Use production Phi2_expr (now implemented as Native custom function)
    Expression result = RandomVariable::Phi2_expr(h, k, rho);
    
    // Cache the result
    phi2_expr_cache[key] = result;
    
    return result;
}

// Bivariate standard normal PDF φ₂(x, y; ρ)
// NOTE: This function is for testing purposes only.
// In production code, expected_prod_pos_expr() in expression.cpp uses direct implementation.
Expression phi2_expr_test(const Expression& x, const Expression& y, const Expression& rho,
                         const Expression& one_minus_rho2, const Expression& sqrt_one_minus_rho2) {
    // φ₂(x, y; ρ) = 1/(2π√(1-ρ²)) × exp(-(x² - 2ρxy + y²)/(2(1-ρ²)))
    
    // Check cache first
    auto key = std::make_tuple(x.get(), y.get(), rho.get());
    auto it = phi2_pdf_expr_cache.find(key);
    if (it != phi2_pdf_expr_cache.end()) {
        return it->second;
    }
    
    // Use provided one_minus_rho2 and sqrt_one_minus_rho2 if available, otherwise compute them
    Expression one_minus_rho2_local;
    Expression sqrt_one_minus_rho2_local;
    
    if (one_minus_rho2 && sqrt_one_minus_rho2) {
        // Use provided values (optimization: avoid redundant computation)
        one_minus_rho2_local = one_minus_rho2;
        sqrt_one_minus_rho2_local = sqrt_one_minus_rho2;
    } else {
        // Compute internally (backward compatibility)
        one_minus_rho2_local = Const(1.0) - rho * rho;
        sqrt_one_minus_rho2_local = sqrt(one_minus_rho2_local);
    }
    
    Expression coeff = Const(1.0) / (Const(2.0 * M_PI) * sqrt_one_minus_rho2_local);
    Expression Q = (x * x - Const(2.0) * rho * x * y + y * y) / one_minus_rho2_local;
    Expression result = coeff * exp(-Q / Const(2.0));
    
    // Cache the result
    phi2_pdf_expr_cache[key] = result;
    
    return result;
}

// Cov(max(0,D0), max(0,D1)) where D0, D1 are bivariate normal
// NOTE: This function is for testing purposes only.
// In production code, cov_max0_max0_expr() in covariance.cpp is used instead.
Expression cov_max0_max0_expr_test(const Expression& mu0, const Expression& sigma0,
                                   const Expression& mu1, const Expression& sigma1,
                                   const Expression& rho) {
    // Cov(D0⁺, D1⁺) = E[D0⁺ D1⁺] - E[D0⁺] × E[D1⁺]
    //
    // E[D0⁺] = expected_positive_part(μ0, σ0) = σ0 × MeanMax(-μ0/σ0) + μ0
    // But using max0_mean_expr for consistency
    Expression E_D0_pos = max0_mean_expr(mu0, sigma0);
    Expression E_D1_pos = max0_mean_expr(mu1, sigma1);

    Expression E_prod = expected_prod_pos_expr(mu0, sigma0, mu1, sigma1, rho);

    return E_prod - E_D0_pos * E_D1_pos;
}

