/**
 * @file test_max0_expr.cpp
 * @brief Tests for MAX0 Expression functions (Phase 2 of #167)
 *
 * Tests:
 * - max0_mean_expr: E[max(0, D)] where D ~ N(μ, σ²)
 * - max0_var_expr: Var[max(0, D)]
 * - Comparison with RandomVariable implementation
 * - Gradient verification
 */

#include <gtest/gtest.h>

#include <cmath>
#include <iomanip>
#include <iostream>

#include "expression.hpp"
#include "statistical_functions.hpp"
#include "max.hpp"
#include "normal.hpp"
#include "util_numerical.hpp"

// Import only statistical functions, not the entire namespace
// to avoid conflict with RandomVariable::RandomVariable type alias
// Note: Variable, Expression, ExpressionImpl are in global namespace
using RandomVariable::phi_expr;
using RandomVariable::Phi_expr;
using RandomVariable::MeanMax_expr;
using RandomVariable::MeanMax2_expr;
using RandomVariable::MeanPhiMax_expr;
using RandomVariable::max0_mean_expr;
using RandomVariable::max0_var_expr;
using RandomVariable::expected_prod_pos_expr;
using RandomVariable::expected_prod_pos_rho1_expr;
using RandomVariable::expected_prod_pos_rho_neg1_expr;

// Numerical gradient using central difference
namespace {
double numerical_gradient(std::function<double(double)> f, double x, double h = 1e-6) {
    return (f(x + h) - f(x - h)) / (2.0 * h);
}
}  // namespace

class Max0ExprTest : public ::testing::Test {
   protected:
    void SetUp() override {
        ExpressionImpl::zero_all_grad();
    }

    void TearDown() override {
        ExpressionImpl::zero_all_grad();
    }
};

// =============================================================================
// max0_mean_expr tests
// =============================================================================

TEST_F(Max0ExprTest, Max0MeanExprValue) {
    // Test that max0_mean_expr matches the RandomVariable implementation
    // Note: Expression uses analytical formula, RandomVariable uses table lookup
    // Table interpolation has ~0.003 max error, so we allow ~0.01 absolute difference
    
    // Test various (μ, σ) combinations
    std::vector<std::pair<double, double>> test_cases = {
        {0.0, 1.0},    // Standard normal
        {1.0, 1.0},    // Positive mean
        {-1.0, 1.0},   // Negative mean
        {0.0, 2.0},    // Larger variance
        {5.0, 2.0},    // Large positive mean (max0 ≈ D)
        {-5.0, 2.0},   // Large negative mean (max0 ≈ 0)
        {10.0, 3.0},   // Extreme positive
        {-10.0, 3.0},  // Extreme negative
    };
    
    for (const auto& [mu, sigma] : test_cases) {
        // RandomVariable implementation (uses table lookup)
        RandomVariable::Normal D(mu, sigma * sigma);
        RandomVariable::RandomVariable max0_rv = RandomVariable::MAX0(D);
        double rv_mean = max0_rv->mean();
        
        // Expression implementation (uses analytical formula)
        Variable mu_expr, sigma_expr;
        mu_expr = mu;
        sigma_expr = sigma;
        Expression mean_expr = max0_mean_expr(mu_expr, sigma_expr);
        double expr_mean = mean_expr->value();
        
        // Allow for table interpolation error (~0.003 * σ)
        double tolerance = 0.01 * sigma;
        EXPECT_NEAR(expr_mean, rv_mean, tolerance)
            << "at μ=" << mu << ", σ=" << sigma;
    }
}

TEST_F(Max0ExprTest, Max0MeanExprGradientMu) {
    // Test gradient w.r.t. μ using Expression's own analytical formula
    Variable mu_expr, sigma_expr;
    
    for (double mu : {-2.0, -1.0, 0.0, 1.0, 2.0}) {
        double sigma = 1.5;
        ExpressionImpl::zero_all_grad();
        
        mu_expr = mu;
        sigma_expr = sigma;
        Expression mean_expr = max0_mean_expr(mu_expr, sigma_expr);
        mean_expr->backward();
        
        // Numerical gradient using the Expression formula itself
        auto f = [sigma](double m) {
            Variable mu_v, sigma_v;
            mu_v = m;
            sigma_v = sigma;
            Expression e = max0_mean_expr(mu_v, sigma_v);
            return e->value();
        };
        double numerical = numerical_gradient(f, mu);
        
        EXPECT_NEAR(mu_expr->gradient(), numerical, 1e-5)
            << "∂mean/∂μ at μ=" << mu;
    }
}

TEST_F(Max0ExprTest, Max0MeanExprGradientSigma) {
    // Test gradient w.r.t. σ using Expression's own analytical formula
    Variable mu_expr, sigma_expr;
    
    for (double sigma : {0.5, 1.0, 1.5, 2.0, 3.0}) {
        double mu = 0.5;
        ExpressionImpl::zero_all_grad();
        
        mu_expr = mu;
        sigma_expr = sigma;
        Expression mean_expr = max0_mean_expr(mu_expr, sigma_expr);
        mean_expr->backward();
        
        // Numerical gradient using the Expression formula itself
        auto f = [mu](double s) {
            Variable mu_v, sigma_v;
            mu_v = mu;
            sigma_v = s;
            Expression e = max0_mean_expr(mu_v, sigma_v);
            return e->value();
        };
        double numerical = numerical_gradient(f, sigma);
        
        EXPECT_NEAR(sigma_expr->gradient(), numerical, 1e-5)
            << "∂mean/∂σ at σ=" << sigma;
    }
}

// =============================================================================
// max0_var_expr tests
// =============================================================================

TEST_F(Max0ExprTest, Max0VarExprValue) {
    // Test that max0_var_expr matches the RandomVariable implementation
    // Note: Expression uses analytical formula, RandomVariable uses table lookup
    // Table interpolation has ~0.003 max error, so we allow ~0.01 relative difference
    
    std::vector<std::pair<double, double>> test_cases = {
        {0.0, 1.0},
        {1.0, 1.0},
        {-1.0, 1.0},
        {0.0, 2.0},
        {5.0, 2.0},
        {-5.0, 2.0},
        {10.0, 3.0},
        {-10.0, 3.0},
    };
    
    for (const auto& [mu, sigma] : test_cases) {
        // RandomVariable implementation (uses table lookup)
        RandomVariable::Normal D(mu, sigma * sigma);
        RandomVariable::RandomVariable max0_rv = RandomVariable::MAX0(D);
        double rv_var = max0_rv->variance();
        
        // Expression implementation (uses analytical formula)
        Variable mu_expr, sigma_expr;
        mu_expr = mu;
        sigma_expr = sigma;
        Expression var_expr = max0_var_expr(mu_expr, sigma_expr);
        double expr_var = var_expr->value();
        
        // Allow for table interpolation error (~0.01 * σ²)
        double tolerance = 0.01 * sigma * sigma;
        EXPECT_NEAR(expr_var, rv_var, tolerance)
            << "at μ=" << mu << ", σ=" << sigma;
    }
}

TEST_F(Max0ExprTest, Max0VarExprGradientMu) {
    // Test gradient w.r.t. μ using Expression's own analytical formula
    Variable mu_expr, sigma_expr;
    
    for (double mu : {-2.0, -1.0, 0.0, 1.0, 2.0}) {
        double sigma = 1.5;
        ExpressionImpl::zero_all_grad();
        
        mu_expr = mu;
        sigma_expr = sigma;
        Expression var_expr = max0_var_expr(mu_expr, sigma_expr);
        var_expr->backward();
        
        // Numerical gradient using the Expression formula itself
        auto f = [sigma](double m) {
            Variable mu_v, sigma_v;
            mu_v = m;
            sigma_v = sigma;
            Expression e = max0_var_expr(mu_v, sigma_v);
            return e->value();
        };
        double numerical = numerical_gradient(f, mu);
        
        EXPECT_NEAR(mu_expr->gradient(), numerical, 1e-4)
            << "∂var/∂μ at μ=" << mu;
    }
}

TEST_F(Max0ExprTest, Max0VarExprGradientSigma) {
    // Test gradient w.r.t. σ using Expression's own analytical formula
    Variable mu_expr, sigma_expr;
    
    for (double sigma : {0.5, 1.0, 1.5, 2.0, 3.0}) {
        double mu = 0.5;
        ExpressionImpl::zero_all_grad();
        
        mu_expr = mu;
        sigma_expr = sigma;
        Expression var_expr = max0_var_expr(mu_expr, sigma_expr);
        var_expr->backward();
        
        // Numerical gradient using the Expression formula itself
        auto f = [mu](double s) {
            Variable mu_v, sigma_v;
            mu_v = mu;
            sigma_v = s;
            Expression e = max0_var_expr(mu_v, sigma_v);
            return e->value();
        };
        double numerical = numerical_gradient(f, sigma);
        
        EXPECT_NEAR(sigma_expr->gradient(), numerical, 1e-4)
            << "∂var/∂σ at σ=" << sigma;
    }
}

// =============================================================================
// Edge case tests
// =============================================================================

TEST_F(Max0ExprTest, Max0ExprBoundaryConditions) {
    Variable mu_expr, sigma_expr;
    
    // When μ >> 0: max(0, D) ≈ D, so mean ≈ μ, var ≈ σ²
    {
        double mu = 10.0, sigma = 1.0;
        mu_expr = mu;
        sigma_expr = sigma;
        
        Expression mean = max0_mean_expr(mu_expr, sigma_expr);
        Expression var = max0_var_expr(mu_expr, sigma_expr);
        
        EXPECT_NEAR(mean->value(), mu, 0.01) << "Large positive μ: mean ≈ μ";
        EXPECT_NEAR(var->value(), sigma * sigma, 0.01) << "Large positive μ: var ≈ σ²";
    }
    
    // When μ << 0: max(0, D) ≈ 0, so mean ≈ 0, var ≈ 0
    {
        double mu = -10.0, sigma = 1.0;
        mu_expr = mu;
        sigma_expr = sigma;
        
        Expression mean = max0_mean_expr(mu_expr, sigma_expr);
        Expression var = max0_var_expr(mu_expr, sigma_expr);
        
        EXPECT_NEAR(mean->value(), 0.0, 0.01) << "Large negative μ: mean ≈ 0";
        EXPECT_NEAR(var->value(), 0.0, 0.01) << "Large negative μ: var ≈ 0";
    }
}

TEST_F(Max0ExprTest, Max0ExprConsistency) {
    // Verify: Var[X] = E[X²] - E[X]²
    // For max0: var = σ²(MeanMax2 - MeanMax²) should be consistent
    
    Variable mu_expr, sigma_expr;
    mu_expr = 0.5;
    sigma_expr = 1.5;
    
    Expression a = -(mu_expr / sigma_expr);  // normalized threshold
    
    Expression mean_max = MeanMax_expr(a);
    Expression mean_max2 = MeanMax2_expr(a);
    
    // var = σ² × (MeanMax2 - MeanMax²)
    Expression var_computed = sigma_expr * sigma_expr * (mean_max2 - mean_max * mean_max);
    Expression var_direct = max0_var_expr(mu_expr, sigma_expr);
    
    EXPECT_NEAR(var_computed->value(), var_direct->value(), 1e-10);
}

// =============================================================================
// Approximation error tests for rho=1 and rho=-1 cases
// =============================================================================

TEST_F(Max0ExprTest, ExpectedProdPosRho1_ApproximationError) {
    // Test approximation error for expected_prod_pos_rho1_expr
    // Compare Expression version (uses smooth min approximation) with
    // exact numerical version (uses exact std::min, no approximation)
    
    std::cout << "\n=== Testing expected_prod_pos_rho1_expr approximation error ===" << std::endl;
    std::cout << std::setprecision(12);
    
    // Test cases: various (a0, a1) combinations where min(a0, a1) matters
    struct TestCase {
        double mu0, sigma0, mu1, sigma1;
        std::string description;
    };
    
    std::vector<TestCase> test_cases = {
        {10.0, 1.0, 10.0, 1.0, "a0 ≈ a1 (equal)"},
        {10.0, 1.0, 10.01, 1.0, "a0 ≈ a1 (very close)"},
        {10.0, 1.0, 10.1, 1.0, "a0 ≈ a1 (close)"},
        {5.0, 1.0, 10.0, 1.0, "a0 < a1 (different)"},
        {10.0, 1.0, 5.0, 1.0, "a0 > a1 (different)"},
        {0.0, 1.0, 0.0, 1.0, "a0 = a1 = 0"},
        {-5.0, 1.0, -10.0, 1.0, "Both negative, a0 > a1"},
        {-10.0, 1.0, -5.0, 1.0, "Both negative, a0 < a1"},
    };
    
    double max_rel_error = 0.0;
    double max_abs_error = 0.0;
    
    for (const auto& tc : test_cases) {
        // Exact numerical version (uses std::min, no approximation)
        double exact_result = RandomVariable::expected_prod_pos_rho1(tc.mu0, tc.sigma0, tc.mu1, tc.sigma1);
        
        // Expression version (approximate, uses smooth min)
        Variable mu0_expr, sigma0_expr, mu1_expr, sigma1_expr;
        mu0_expr = tc.mu0;
        sigma0_expr = tc.sigma0;
        mu1_expr = tc.mu1;
        sigma1_expr = tc.sigma1;
        
        Expression expr_result = expected_prod_pos_rho1_expr(mu0_expr, sigma0_expr, mu1_expr, sigma1_expr);
        double expr_val = expr_result->value();
        
        double abs_error = std::abs(expr_val - exact_result);
        double rel_error = (std::abs(exact_result) > 1e-15) ? abs_error / std::abs(exact_result) : abs_error;
        
        max_rel_error = std::max(max_rel_error, rel_error);
        max_abs_error = std::max(max_abs_error, abs_error);
        
        double a0 = tc.mu0 / tc.sigma0;
        double a1 = tc.mu1 / tc.sigma1;
        double diff = std::abs(a0 - a1);
        
        std::cout << "\n" << tc.description << ":" << std::endl;
        std::cout << "  μ0=" << tc.mu0 << ", σ0=" << tc.sigma0 
                  << ", μ1=" << tc.mu1 << ", σ1=" << tc.sigma1 << std::endl;
        std::cout << "  a0=" << a0 << ", a1=" << a1 << ", |a0-a1|=" << diff << std::endl;
        std::cout << "  Exact (std::min): " << exact_result << std::endl;
        std::cout << "  Expression (approx): " << expr_val << std::endl;
        std::cout << "  Abs error: " << abs_error << std::endl;
        std::cout << "  Rel error: " << rel_error << std::endl;
        
        // For epsilon=1e-10, expect error ~1e-5 when |a0-a1| ≈ 0
        // For |a0-a1| >> 1e-5, error should be negligible (< 1e-10)
        if (diff < 1e-5) {
            EXPECT_LT(rel_error, 1e-4) << "When a0 ≈ a1, rel error should be < 1e-4";
        } else {
            EXPECT_LT(rel_error, 1e-8) << "When |a0-a1| >> 1e-5, rel error should be < 1e-8";
        }
    }
    
    std::cout << "\n=== Summary ===" << std::endl;
    std::cout << "Max relative error: " << max_rel_error << std::endl;
    std::cout << "Max absolute error: " << max_abs_error << std::endl;
}

TEST_F(Max0ExprTest, ExpectedProdPosRhoNeg1_ApproximationError) {
    // Test approximation error for expected_prod_pos_rho_neg1_expr
    // Compare Expression version (uses smooth step function) with
    // exact numerical version (uses exact condition check, no approximation)
    
    std::cout << "\n=== Testing expected_prod_pos_rho_neg1_expr approximation error ===" << std::endl;
    std::cout << std::setprecision(12);
    
    struct TestCase {
        double mu0, sigma0, mu1, sigma1;
        std::string description;
    };
    
    std::vector<TestCase> test_cases = {
        {10.0, 1.0, 10.0, 1.0, "a0 + a1 = 20 > 0"},
        {5.0, 1.0, 5.0, 1.0, "a0 + a1 = 10 > 0"},
        {1.0, 1.0, 1.0, 1.0, "a0 + a1 = 2 > 0"},
        {0.1, 1.0, 0.1, 1.0, "a0 + a1 = 0.2 > 0 (small positive)"},
        {0.0, 1.0, 0.0, 1.0, "a0 + a1 = 0 (boundary)"},
        {-0.1, 1.0, -0.1, 1.0, "a0 + a1 = -0.2 < 0 (should be 0)"},
        {-1.0, 1.0, -1.0, 1.0, "a0 + a1 = -2 < 0 (should be 0)"},
        {-5.0, 1.0, -5.0, 1.0, "a0 + a1 = -10 < 0 (should be 0)"},
        {10.0, 1.0, -5.0, 1.0, "a0 + a1 = 5 > 0"},
        {-5.0, 1.0, 10.0, 1.0, "a0 + a1 = 5 > 0"},
        {1e-6, 1.0, 1e-6, 1.0, "a0 + a1 = 2e-6 > 0 (very small positive)"},
        {-1e-6, 1.0, -1e-6, 1.0, "a0 + a1 = -2e-6 < 0 (very small negative)"},
    };
    
    double max_rel_error = 0.0;
    double max_abs_error = 0.0;
    
    for (const auto& tc : test_cases) {
        // Exact numerical version (uses exact condition check, no approximation)
        double exact_result = RandomVariable::expected_prod_pos_rho_neg1(tc.mu0, tc.sigma0, tc.mu1, tc.sigma1);
        
        // Expression version (approximate, uses smooth step function)
        Variable mu0_expr, sigma0_expr, mu1_expr, sigma1_expr;
        mu0_expr = tc.mu0;
        sigma0_expr = tc.sigma0;
        mu1_expr = tc.mu1;
        sigma1_expr = tc.sigma1;
        
        Expression expr_result = expected_prod_pos_rho_neg1_expr(mu0_expr, sigma0_expr, mu1_expr, sigma1_expr);
        double expr_val = expr_result->value();
        
        double abs_error = std::abs(expr_val - exact_result);
        double rel_error = (std::abs(exact_result) > 1e-15) ? abs_error / std::abs(exact_result) : abs_error;
        
        max_rel_error = std::max(max_rel_error, rel_error);
        max_abs_error = std::max(max_abs_error, abs_error);
        
        double a0 = tc.mu0 / tc.sigma0;
        double a1 = tc.mu1 / tc.sigma1;
        double sum = a0 + a1;
        
        std::cout << "\n" << tc.description << ":" << std::endl;
        std::cout << "  μ0=" << tc.mu0 << ", σ0=" << tc.sigma0 
                  << ", μ1=" << tc.mu1 << ", σ1=" << tc.sigma1 << std::endl;
        std::cout << "  a0 + a1 = " << sum << std::endl;
        std::cout << "  Exact (condition check): " << exact_result << std::endl;
        std::cout << "  Expression (approx): " << expr_val << std::endl;
        std::cout << "  Abs error: " << abs_error << std::endl;
        std::cout << "  Rel error: " << rel_error << std::endl;
        
        // For a0 + a1 >> epsilon, step function should be accurate
        // For a0 + a1 ≈ 0, expect smooth transition
        if (sum > 1e-5) {
            // Well in positive region, should be accurate
            EXPECT_LT(rel_error, 1e-6) << "When a0+a1 >> epsilon, rel error should be < 1e-6";
        } else if (sum < -1e-5) {
            // Well in negative region, should be close to 0
            EXPECT_LT(expr_val, 1e-5) << "When a0+a1 << -epsilon, result should be ≈ 0";
        } else {
            // Near boundary, allow larger error due to smooth transition
            EXPECT_LT(abs_error, 1e-3) << "Near boundary (|a0+a1| < 1e-5), abs error should be < 1e-3";
        }
    }
    
    std::cout << "\n=== Summary ===" << std::endl;
    std::cout << "Max relative error: " << max_rel_error << std::endl;
    std::cout << "Max absolute error: " << max_abs_error << std::endl;
}

