/**
 * @file test_expression_math.cpp
 * @brief Tests for Expression mathematical functions (Phase 1 of #167)
 *
 * Tests:
 * - ERF operation and its gradient
 * - SQRT operation and its gradient
 * - phi_expr (standard normal PDF)
 * - Phi_expr (standard normal CDF)
 * - MeanMax_expr, MeanMax2_expr, MeanPhiMax_expr
 * - Numerical gradient verification
 */

#include <gtest/gtest.h>

#include <cmath>
#include <iomanip>
#include <iostream>

#include "expression.hpp"
#include "statistical_functions.hpp"

using namespace RandomVariable;

// Reference implementations for comparison
namespace {

double normal_pdf(double x) {
    static constexpr double INV_SQRT_2PI = 0.3989422804014327;
    return INV_SQRT_2PI * std::exp(-0.5 * x * x);
}

double normal_cdf(double x) {
    return 0.5 * std::erfc(-x / std::sqrt(2.0));
}

double MeanMax_ref(double a) {
    return normal_pdf(a) + a * normal_cdf(a);
}

double MeanMax2_ref(double a) {
    return 1.0 + (a * a - 1.0) * normal_cdf(a) + a * normal_pdf(a);
}

double MeanPhiMax_ref(double a) {
    return 1.0 - normal_cdf(a);
}

// Numerical gradient using central difference
double numerical_gradient(std::function<double(double)> f, double x, double h = 1e-6) {
    return (f(x + h) - f(x - h)) / (2.0 * h);
}

}  // namespace

class ExpressionMathTest : public ::testing::Test {
   protected:
    void SetUp() override {
        ExpressionImpl::zero_all_grad();
    }

    void TearDown() override {
        ExpressionImpl::zero_all_grad();
    }
};

// =============================================================================
// ERF operation tests
// =============================================================================

TEST_F(ExpressionMathTest, ErfValue) {
    Variable x;
    x = 0.0;
    Expression f = erf(x);
    EXPECT_NEAR(f->value(), 0.0, 1e-10);

    x = 1.0;
    EXPECT_NEAR(f->value(), std::erf(1.0), 1e-10);

    x = -1.0;
    EXPECT_NEAR(f->value(), std::erf(-1.0), 1e-10);
}

TEST_F(ExpressionMathTest, ErfGradient) {
    Variable x;
    x = 0.5;
    Expression f = erf(x);
    f->backward();

    // ∂erf/∂x = 2/√π × exp(-x²)
    double expected = (2.0 / std::sqrt(M_PI)) * std::exp(-0.5 * 0.5);
    EXPECT_NEAR(x->gradient(), expected, 1e-10);
}

TEST_F(ExpressionMathTest, ErfGradientNumerical) {
    Variable x;
    
    for (double val : {-2.0, -1.0, -0.5, 0.0, 0.5, 1.0, 2.0}) {
        ExpressionImpl::zero_all_grad();
        x = val;
        Expression f = erf(x);
        f->backward();
        
        double numerical = numerical_gradient([](double v) { return std::erf(v); }, val);
        EXPECT_NEAR(x->gradient(), numerical, 1e-5) << "at x=" << val;
    }
}

// =============================================================================
// SQRT operation tests
// =============================================================================

TEST_F(ExpressionMathTest, SqrtValue) {
    Variable x;
    x = 4.0;
    Expression f = sqrt(x);
    EXPECT_NEAR(f->value(), 2.0, 1e-10);

    x = 9.0;
    EXPECT_NEAR(f->value(), 3.0, 1e-10);
}

TEST_F(ExpressionMathTest, SqrtGradient) {
    Variable x;
    x = 4.0;
    Expression f = sqrt(x);
    f->backward();

    // ∂sqrt(x)/∂x = 1/(2√x)
    double expected = 1.0 / (2.0 * 2.0);  // 1/(2*sqrt(4)) = 0.25
    EXPECT_NEAR(x->gradient(), expected, 1e-10);
}

TEST_F(ExpressionMathTest, SqrtGradientNumerical) {
    Variable x;
    
    for (double val : {0.25, 1.0, 4.0, 9.0, 16.0}) {
        ExpressionImpl::zero_all_grad();
        x = val;
        Expression f = sqrt(x);
        f->backward();
        
        double numerical = numerical_gradient([](double v) { return std::sqrt(v); }, val);
        EXPECT_NEAR(x->gradient(), numerical, 1e-5) << "at x=" << val;
    }
}

// =============================================================================
// phi_expr (normal PDF) tests
// =============================================================================

TEST_F(ExpressionMathTest, PhiExprValue) {
    Variable x;
    
    for (double val : {-2.0, -1.0, 0.0, 1.0, 2.0}) {
        x = val;
        Expression f = phi_expr(x);
        double expected = normal_pdf(val);
        EXPECT_NEAR(f->value(), expected, 1e-10) << "at x=" << val;
    }
}

TEST_F(ExpressionMathTest, PhiExprGradient) {
    Variable x;
    
    // φ(x) = exp(-x²/2) / √(2π)
    // ∂φ/∂x = -x × φ(x)
    for (double val : {-2.0, -1.0, 0.0, 1.0, 2.0}) {
        ExpressionImpl::zero_all_grad();
        x = val;
        Expression f = phi_expr(x);
        f->backward();
        
        double expected = -val * normal_pdf(val);
        EXPECT_NEAR(x->gradient(), expected, 1e-9) << "at x=" << val;
    }
}

// =============================================================================
// Phi_expr (normal CDF) tests
// =============================================================================

TEST_F(ExpressionMathTest, PhiCdfExprValue) {
    Variable x;
    
    for (double val : {-2.0, -1.0, 0.0, 1.0, 2.0}) {
        x = val;
        Expression f = Phi_expr(x);
        double expected = normal_cdf(val);
        EXPECT_NEAR(f->value(), expected, 1e-10) << "at x=" << val;
    }
}

TEST_F(ExpressionMathTest, PhiCdfExprGradient) {
    Variable x;
    
    // Φ(x) = 0.5 × (1 + erf(x/√2))
    // ∂Φ/∂x = φ(x)
    for (double val : {-2.0, -1.0, 0.0, 1.0, 2.0}) {
        ExpressionImpl::zero_all_grad();
        x = val;
        Expression f = Phi_expr(x);
        f->backward();
        
        double expected = normal_pdf(val);
        EXPECT_NEAR(x->gradient(), expected, 1e-9) << "at x=" << val;
    }
}

// =============================================================================
// MeanMax_expr tests
// =============================================================================

TEST_F(ExpressionMathTest, MeanMaxExprValue) {
    Variable a;
    
    for (double val : {-3.0, -2.0, -1.0, 0.0, 1.0, 2.0, 3.0}) {
        a = val;
        Expression f = MeanMax_expr(a);
        double expected = MeanMax_ref(val);
        EXPECT_NEAR(f->value(), expected, 1e-9) << "at a=" << val;
    }
}

TEST_F(ExpressionMathTest, MeanMaxExprGradientNumerical) {
    Variable a;
    
    for (double val : {-2.0, -1.0, 0.0, 1.0, 2.0}) {
        ExpressionImpl::zero_all_grad();
        a = val;
        Expression f = MeanMax_expr(a);
        f->backward();
        
        double numerical = numerical_gradient(MeanMax_ref, val);
        EXPECT_NEAR(a->gradient(), numerical, 1e-5) << "at a=" << val;
    }
}

// =============================================================================
// MeanMax2_expr tests
// =============================================================================

TEST_F(ExpressionMathTest, MeanMax2ExprValue) {
    Variable a;
    
    for (double val : {-3.0, -2.0, -1.0, 0.0, 1.0, 2.0, 3.0}) {
        a = val;
        Expression f = MeanMax2_expr(a);
        double expected = MeanMax2_ref(val);
        EXPECT_NEAR(f->value(), expected, 1e-9) << "at a=" << val;
    }
}

TEST_F(ExpressionMathTest, MeanMax2ExprGradientNumerical) {
    Variable a;
    
    for (double val : {-2.0, -1.0, 0.0, 1.0, 2.0}) {
        ExpressionImpl::zero_all_grad();
        a = val;
        Expression f = MeanMax2_expr(a);
        f->backward();
        
        double numerical = numerical_gradient(MeanMax2_ref, val);
        EXPECT_NEAR(a->gradient(), numerical, 1e-5) << "at a=" << val;
    }
}

// =============================================================================
// MeanPhiMax_expr tests
// =============================================================================

TEST_F(ExpressionMathTest, MeanPhiMaxExprValue) {
    Variable a;
    
    for (double val : {-3.0, -2.0, -1.0, 0.0, 1.0, 2.0, 3.0}) {
        a = val;
        Expression f = MeanPhiMax_expr(a);
        double expected = MeanPhiMax_ref(val);
        EXPECT_NEAR(f->value(), expected, 1e-9) << "at a=" << val;
    }
}

TEST_F(ExpressionMathTest, MeanPhiMaxExprGradientNumerical) {
    Variable a;
    
    for (double val : {-2.0, -1.0, 0.0, 1.0, 2.0}) {
        ExpressionImpl::zero_all_grad();
        a = val;
        Expression f = MeanPhiMax_expr(a);
        f->backward();
        
        double numerical = numerical_gradient(MeanPhiMax_ref, val);
        EXPECT_NEAR(a->gradient(), numerical, 1e-5) << "at a=" << val;
    }
}

// =============================================================================
// Composite expression tests
// =============================================================================

TEST_F(ExpressionMathTest, CompositeExpression) {
    // Test a composite expression using multiple new operations
    Variable x;
    x = 1.0;
    
    // f = sqrt(1 + erf(x)^2)
    Expression f = sqrt(1.0 + (erf(x) ^ 2.0));
    
    // Verify value
    double erf_val = std::erf(1.0);
    double expected = std::sqrt(1.0 + erf_val * erf_val);
    EXPECT_NEAR(f->value(), expected, 1e-10);
    
    // Verify gradient numerically
    f->backward();
    auto composite_func = [](double v) {
        double e = std::erf(v);
        return std::sqrt(1.0 + e * e);
    };
    double numerical = numerical_gradient(composite_func, 1.0);
    EXPECT_NEAR(x->gradient(), numerical, 1e-5);
}

TEST_F(ExpressionMathTest, ChainRule) {
    // f = Phi(phi(x)) - chain of two statistical functions
    Variable x;
    x = 0.5;
    
    Expression f = Phi_expr(phi_expr(x));
    f->backward();
    
    // Numerical verification
    auto chain_func = [](double v) {
        return normal_cdf(normal_pdf(v));
    };
    double numerical = numerical_gradient(chain_func, 0.5);
    EXPECT_NEAR(x->gradient(), numerical, 1e-5);
}

