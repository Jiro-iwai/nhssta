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

