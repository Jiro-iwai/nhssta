// -*- c++ -*-
// Test cases for Expression automatic differentiation (reverse-mode AD)
// Author: Jiro Iwai

#include <gtest/gtest.h>

#include <cmath>

#include "../src/expression.hpp"

class ExpressionDerivativeTest : public ::testing::Test {
   protected:
    void SetUp() override {
        // Clear all gradients before each test
        zero_all_grad();
    }

    void TearDown() override {
        // Clean up
        zero_all_grad();
    }

    // Helper function for numerical differentiation (central difference)
    static double numerical_derivative(const std::function<double(double)>& f, double x,
                                       double h = 1e-5) {
        return (f(x + h) - f(x - h)) / (2 * h);
    }
};

// Test basic addition gradient
TEST_F(ExpressionDerivativeTest, AdditionGradient) {
    Variable x, y;
    x = 2.0;
    y = 3.0;

    Expression f = x + y;  // f = x + y = 5

    EXPECT_DOUBLE_EQ(f->value(), 5.0);

    // Compute gradients
    f->backward();

    // ∂f/∂x = 1, ∂f/∂y = 1
    EXPECT_DOUBLE_EQ(x->gradient(), 1.0);
    EXPECT_DOUBLE_EQ(y->gradient(), 1.0);
}

// Test subtraction gradient
TEST_F(ExpressionDerivativeTest, SubtractionGradient) {
    Variable x, y;
    x = 5.0;
    y = 3.0;

    Expression f = x - y;  // f = x - y = 2

    EXPECT_DOUBLE_EQ(f->value(), 2.0);

    f->backward();

    // ∂f/∂x = 1, ∂f/∂y = -1
    EXPECT_DOUBLE_EQ(x->gradient(), 1.0);
    EXPECT_DOUBLE_EQ(y->gradient(), -1.0);
}

// Test multiplication gradient
TEST_F(ExpressionDerivativeTest, MultiplicationGradient) {
    Variable x, y;
    x = 2.0;
    y = 3.0;

    Expression f = x * y;  // f = x * y = 6

    EXPECT_DOUBLE_EQ(f->value(), 6.0);

    f->backward();

    // ∂f/∂x = y = 3, ∂f/∂y = x = 2
    EXPECT_DOUBLE_EQ(x->gradient(), 3.0);
    EXPECT_DOUBLE_EQ(y->gradient(), 2.0);
}

// Test division gradient
TEST_F(ExpressionDerivativeTest, DivisionGradient) {
    Variable x, y;
    x = 6.0;
    y = 2.0;

    Expression f = x / y;  // f = x / y = 3

    EXPECT_DOUBLE_EQ(f->value(), 3.0);

    f->backward();

    // ∂f/∂x = 1/y = 0.5, ∂f/∂y = -x/y² = -6/4 = -1.5
    EXPECT_DOUBLE_EQ(x->gradient(), 0.5);
    EXPECT_DOUBLE_EQ(y->gradient(), -1.5);
}

// Test power gradient
TEST_F(ExpressionDerivativeTest, PowerGradient) {
    Variable x;
    x = 2.0;

    Expression f = x ^ 3.0;  // f = x³ = 8

    EXPECT_DOUBLE_EQ(f->value(), 8.0);

    f->backward();

    // ∂f/∂x = 3x² = 12
    EXPECT_DOUBLE_EQ(x->gradient(), 12.0);
}

// Test power gradient with variable exponent
TEST_F(ExpressionDerivativeTest, PowerGradientWithVariableExponent) {
    Variable x, y;
    x = 2.0;
    y = 3.0;

    Expression f = x ^ y;  // f = x^y = 8

    EXPECT_DOUBLE_EQ(f->value(), 8.0);

    f->backward();

    // ∂f/∂x = y * x^(y-1) = 3 * 4 = 12
    // ∂f/∂y = x^y * log(x) = 8 * log(2) ≈ 5.545
    EXPECT_DOUBLE_EQ(x->gradient(), 12.0);
    EXPECT_NEAR(y->gradient(), 8.0 * std::log(2.0), 1e-10);
}

// Test exponential gradient
TEST_F(ExpressionDerivativeTest, ExponentialGradient) {
    Variable x;
    x = 1.0;

    Expression f = exp(x);  // f = e^1 = e

    EXPECT_NEAR(f->value(), std::exp(1.0), 1e-10);

    f->backward();

    // ∂f/∂x = exp(x) = e
    EXPECT_NEAR(x->gradient(), std::exp(1.0), 1e-10);
}

// Test logarithm gradient
TEST_F(ExpressionDerivativeTest, LogarithmGradient) {
    Variable x;
    x = 2.0;

    Expression f = log(x);  // f = log(2)

    EXPECT_NEAR(f->value(), std::log(2.0), 1e-10);

    f->backward();

    // ∂f/∂x = 1/x = 0.5
    EXPECT_DOUBLE_EQ(x->gradient(), 0.5);
}

// Test chain rule: f = (x + y) * z
TEST_F(ExpressionDerivativeTest, ChainRuleAddMul) {
    Variable x, y, z;
    x = 2.0;
    y = 3.0;
    z = 4.0;

    Expression f = (x + y) * z;  // f = (2 + 3) * 4 = 20

    EXPECT_DOUBLE_EQ(f->value(), 20.0);

    f->backward();

    // ∂f/∂x = z = 4
    // ∂f/∂y = z = 4
    // ∂f/∂z = x + y = 5
    EXPECT_DOUBLE_EQ(x->gradient(), 4.0);
    EXPECT_DOUBLE_EQ(y->gradient(), 4.0);
    EXPECT_DOUBLE_EQ(z->gradient(), 5.0);
}

// Test chain rule: f = x * y + x^2
TEST_F(ExpressionDerivativeTest, ChainRuleWithSharedVariable) {
    Variable x, y;
    x = 2.0;
    y = 3.0;

    Expression f = x * y + (x ^ 2.0);  // f = 2*3 + 4 = 10

    EXPECT_DOUBLE_EQ(f->value(), 10.0);

    f->backward();

    // ∂f/∂x = y + 2x = 3 + 4 = 7
    // ∂f/∂y = x = 2
    EXPECT_DOUBLE_EQ(x->gradient(), 7.0);
    EXPECT_DOUBLE_EQ(y->gradient(), 2.0);
}

// Test complex expression: f = exp(x * y) / z
TEST_F(ExpressionDerivativeTest, ComplexExpression) {
    Variable x, y, z;
    x = 1.0;
    y = 2.0;
    z = 3.0;

    Expression f = exp(x * y) / z;  // f = exp(2) / 3

    double expected_value = std::exp(2.0) / 3.0;
    EXPECT_NEAR(f->value(), expected_value, 1e-10);

    f->backward();

    // f = exp(x*y) / z
    // ∂f/∂x = y * exp(x*y) / z = 2 * exp(2) / 3
    // ∂f/∂y = x * exp(x*y) / z = 1 * exp(2) / 3
    // ∂f/∂z = -exp(x*y) / z² = -exp(2) / 9
    double exp2 = std::exp(2.0);
    EXPECT_NEAR(x->gradient(), 2.0 * exp2 / 3.0, 1e-10);
    EXPECT_NEAR(y->gradient(), 1.0 * exp2 / 3.0, 1e-10);
    EXPECT_NEAR(z->gradient(), -exp2 / 9.0, 1e-10);
}

// Test zero_grad clears gradients
TEST_F(ExpressionDerivativeTest, ZeroGradWorks) {
    Variable x;
    x = 2.0;

    Expression f = x * x;
    f->backward();

    EXPECT_DOUBLE_EQ(x->gradient(), 4.0);

    // Clear gradients
    zero_all_grad();

    EXPECT_DOUBLE_EQ(x->gradient(), 0.0);
}

// Test multiple backward passes (with zero_grad between)
TEST_F(ExpressionDerivativeTest, MultipleBackwardPasses) {
    Variable x;
    x = 2.0;

    Expression f1 = x * x;  // f1 = x²
    f1->backward();
    EXPECT_DOUBLE_EQ(x->gradient(), 4.0);  // ∂(x²)/∂x = 2x = 4

    zero_all_grad();

    x = 3.0;
    Expression f2 = x * x * x;  // f2 = x³
    f2->backward();
    EXPECT_DOUBLE_EQ(x->gradient(), 27.0);  // ∂(x³)/∂x = 3x² = 27
}

// Test numerical gradient verification
TEST_F(ExpressionDerivativeTest, NumericalGradientVerification) {
    Variable x, y;

    // Test f = sin approximation: x - x³/6 + x⁵/120
    // For simplicity, test f = x³ - 2x²+ 3x - 1
    auto f_lambda = [](double val) { return val * val * val - 2 * val * val + 3 * val - 1; };

    double test_x = 2.5;
    x = test_x;

    // f = x³ - 2x² + 3x - 1
    Expression f = (x ^ 3.0) - 2.0 * (x ^ 2.0) + 3.0 * x - 1.0;

    f->backward();

    // Analytical: ∂f/∂x = 3x² - 4x + 3
    double analytical_grad = 3 * test_x * test_x - 4 * test_x + 3;
    EXPECT_NEAR(x->gradient(), analytical_grad, 1e-10);

    // Numerical gradient
    double numerical_grad = numerical_derivative(f_lambda, test_x);
    EXPECT_NEAR(x->gradient(), numerical_grad, 1e-5);
}

// Test gradient accumulation when variable is used multiple times
TEST_F(ExpressionDerivativeTest, GradientAccumulation) {
    Variable x;
    x = 3.0;

    // f = x + x + x = 3x
    Expression f = x + x + x;

    EXPECT_DOUBLE_EQ(f->value(), 9.0);

    f->backward();

    // ∂f/∂x = 3
    EXPECT_DOUBLE_EQ(x->gradient(), 3.0);
}

// Test with constant expressions
TEST_F(ExpressionDerivativeTest, ConstantExpressions) {
    Variable x;
    x = 2.0;

    // f = 5 * x + 3
    Expression f = 5.0 * x + 3.0;

    EXPECT_DOUBLE_EQ(f->value(), 13.0);

    f->backward();

    // ∂f/∂x = 5
    EXPECT_DOUBLE_EQ(x->gradient(), 5.0);
}

// Test nested operations
TEST_F(ExpressionDerivativeTest, NestedOperations) {
    Variable x;
    x = 2.0;

    // f = log(exp(x)) = x (should have gradient 1)
    Expression f = log(exp(x));

    EXPECT_NEAR(f->value(), 2.0, 1e-10);

    f->backward();

    // ∂f/∂x = 1
    EXPECT_NEAR(x->gradient(), 1.0, 1e-10);
}

// Test unary minus
TEST_F(ExpressionDerivativeTest, UnaryMinus) {
    Variable x;
    x = 3.0;

    Expression f = -x;

    EXPECT_DOUBLE_EQ(f->value(), -3.0);

    f->backward();

    // ∂(-x)/∂x = -1
    EXPECT_DOUBLE_EQ(x->gradient(), -1.0);
}

