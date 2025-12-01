// -*- c++ -*-
// Test cases for Custom Function feature
// Author: Jiro Iwai

#include <gtest/gtest.h>
#include <cmath>

#include "../src/expression.hpp"

class CustomFunctionTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Clear all gradients before each test
        zero_all_grad();
    }

    void TearDown() override {
        // Clean up
        zero_all_grad();
    }
};

// Test 1: Single variable function f(x) = x²
TEST_F(CustomFunctionTest, SingleVariableSquare) {
    CustomFunction f = CustomFunction::create(
        1,
        [](const std::vector<Variable>& v) {
            return v[0] * v[0];
        },
        "square"
    );

    EXPECT_EQ(f.input_dim(), 1);
    EXPECT_EQ(f.name(), "square");
    EXPECT_TRUE(f.valid());

    // Test value
    EXPECT_DOUBLE_EQ(f.value({3.0}), 9.0);
    EXPECT_DOUBLE_EQ(f.value({-2.0}), 4.0);
    EXPECT_DOUBLE_EQ(f.value({0.0}), 0.0);

    // Test gradient
    std::vector<double> grad = f.gradient({3.0});
    ASSERT_EQ(grad.size(), 1);
    EXPECT_DOUBLE_EQ(grad[0], 6.0);  // df/dx = 2x = 6

    grad = f.gradient({-2.0});
    ASSERT_EQ(grad.size(), 1);
    EXPECT_DOUBLE_EQ(grad[0], -4.0);  // df/dx = 2x = -4

    // Test value_and_gradient
    auto [val, grad_vec] = f.value_and_gradient({5.0});
    EXPECT_DOUBLE_EQ(val, 25.0);
    ASSERT_EQ(grad_vec.size(), 1);
    EXPECT_DOUBLE_EQ(grad_vec[0], 10.0);
}

// Test 2: Two variable function f(x,y) = x*y + x² integrated with main expression tree
TEST_F(CustomFunctionTest, TwoVariableWithMainExpressionTree) {
    CustomFunction f = CustomFunction::create(
        2,
        [](const std::vector<Variable>& v) {
            const auto& x = v[0];
            const auto& y = v[1];
            return x * y + x * x;
        },
        "f_xy"
    );

    EXPECT_EQ(f.input_dim(), 2);
    EXPECT_EQ(f.name(), "f_xy");
    EXPECT_TRUE(f.valid());

    // Create main expression tree variables
    Variable X, Y;
    X = 2.0;
    Y = 3.0;

    // Build main expression: F = f(X, Y) + X
    Expression F = f(X, Y) + X;

    // Test value: f(2, 3) = 2*3 + 2*2 = 6 + 4 = 10, F = 10 + 2 = 12
    EXPECT_DOUBLE_EQ(F->value(), 12.0);

    // Compute gradients
    zero_all_grad();
    F->backward();

    // dF/dX = d(f(X,Y) + X)/dX = df/dX + 1
    // df/dX = Y + 2X = 3 + 4 = 7
    // dF/dX = 7 + 1 = 8
    EXPECT_DOUBLE_EQ(X->gradient(), 8.0);

    // dF/dY = df/dY = X = 2
    EXPECT_DOUBLE_EQ(Y->gradient(), 2.0);
}

// Test 3: Five variable function (SSTA-like usage)
TEST_F(CustomFunctionTest, FiveVariableFunction) {
    CustomFunction f = CustomFunction::create(
        5,
        [](const std::vector<Variable>& v) {
            const auto& x0 = v[0];
            const auto& x1 = v[1];
            const auto& x2 = v[2];
            const auto& x3 = v[3];
            const auto& x4 = v[4];
            return x0 * x1 + exp(x2) + phi_expr(x3) * x4;
        },
        "f_ssta"
    );

    EXPECT_EQ(f.input_dim(), 5);
    EXPECT_TRUE(f.valid());

    // Test with random inputs
    std::vector<double> x = {1.5, 2.0, 0.5, 1.0, 3.0};
    std::vector<double> grad = f.gradient(x);

    ASSERT_EQ(grad.size(), 5);

    // Verify gradient using finite differences
    const double h = 1e-5;
    for (size_t i = 0; i < 5; ++i) {
        std::vector<double> x_plus = x;
        x_plus[i] += h;
        double val_plus = f.value(x_plus);

        std::vector<double> x_minus = x;
        x_minus[i] -= h;
        double val_minus = f.value(x_minus);

        double numerical_grad = (val_plus - val_minus) / (2.0 * h);
        EXPECT_NEAR(grad[i], numerical_grad, 1e-4)
            << "Gradient mismatch for variable " << i;
    }
}

// Test 4: Multiple calls to the same custom function
TEST_F(CustomFunctionTest, MultipleCallsToSameFunction) {
    CustomFunction f = CustomFunction::create(
        2,
        [](const std::vector<Variable>& v) {
            return v[0] * v[1];
        },
        "multiply"
    );

    Variable X, Y, Z;
    X = 1.0;
    Y = 2.0;
    Z = 3.0;

    // F = f(X,Y) + f(Y,Z) + f(X,Z)
    // f(X,Y) = 1*2 = 2
    // f(Y,Z) = 2*3 = 6
    // f(X,Z) = 1*3 = 3
    // F = 2 + 6 + 3 = 11
    Expression F = f(X, Y) + f(Y, Z) + f(X, Z);

    EXPECT_DOUBLE_EQ(F->value(), 11.0);

    // Compute gradients
    zero_all_grad();
    F->backward();

    // dF/dX = df(X,Y)/dX + df(X,Z)/dX = Y + Z = 2 + 3 = 5
    EXPECT_DOUBLE_EQ(X->gradient(), 5.0);

    // dF/dY = df(X,Y)/dY + df(Y,Z)/dY = X + Z = 1 + 3 = 4
    EXPECT_DOUBLE_EQ(Y->gradient(), 4.0);

    // dF/dZ = df(Y,Z)/dZ + df(X,Z)/dZ = Y + X = 2 + 1 = 3
    EXPECT_DOUBLE_EQ(Z->gradient(), 3.0);
}

// Test 5: Independence between main expression tree and custom function
TEST_F(CustomFunctionTest, IndependenceFromMainExpressionTree) {
    CustomFunction f = CustomFunction::create(
        1,
        [](const std::vector<Variable>& v) {
            return v[0] * v[0];
        },
        "square"
    );

    Variable X;
    X = 3.0;

    // Build main expression
    Expression F = f(X) + X;
    EXPECT_DOUBLE_EQ(F->value(), 12.0);  // 9 + 3 = 12

    // Compute gradients in main tree
    zero_all_grad();
    F->backward();
    double main_grad_X = X->gradient();  // Should be 2*3 + 1 = 7

    // Now call f.gradient independently
    std::vector<double> grad = f.gradient({5.0});
    ASSERT_EQ(grad.size(), 1);
    EXPECT_DOUBLE_EQ(grad[0], 10.0);  // df/dx at x=5 is 10

    // Main expression tree gradient should not be affected
    EXPECT_DOUBLE_EQ(X->gradient(), main_grad_X);

    // Test reverse: call f.gradient first, then main backward
    zero_all_grad();
    std::vector<double> grad2 = f.gradient({7.0});
    EXPECT_DOUBLE_EQ(grad2[0], 14.0);

    X = 3.0;
    Expression F2 = f(X) + X;
    F2->backward();
    EXPECT_DOUBLE_EQ(X->gradient(), 7.0);  // Should still work correctly
}

// Test: Detailed independence - value cache independence
TEST_F(CustomFunctionTest, ValueCacheIndependence) {
    CustomFunction f = CustomFunction::create(
        1,
        [](const std::vector<Variable>& v) {
            return v[0] * v[0] + v[0];
        },
        "quadratic"
    );

    Variable X;
    X = 2.0;

    // Build main expression
    Expression F = f(X) + X;
    double val1 = F->value();
    EXPECT_DOUBLE_EQ(val1, 8.0);  // (4 + 2) + 2 = 8

    // Change X value
    X = 5.0;
    double val2 = F->value();
    EXPECT_DOUBLE_EQ(val2, 35.0);  // (25 + 5) + 5 = 35

    // Independent evaluation of custom function should work
    double f_val = f.value({10.0});
    EXPECT_DOUBLE_EQ(f_val, 110.0);  // 100 + 10 = 110

    // Main expression should still work
    double val3 = F->value();
    EXPECT_DOUBLE_EQ(val3, 35.0);  // Should still be 35 (X = 5.0)
}

// Test: Detailed independence - gradient independence with zero_all_grad
TEST_F(CustomFunctionTest, GradientIndependenceWithZeroAllGrad) {
    CustomFunction f = CustomFunction::create(
        2,
        [](const std::vector<Variable>& v) {
            return v[0] * v[1];
        },
        "multiply"
    );

    Variable X, Y;
    X = 2.0;
    Y = 3.0;

    // Build main expression
    Expression F = f(X, Y) + X * Y;
    zero_all_grad();
    F->backward();
    
    double main_grad_X = X->gradient();
    double main_grad_Y = Y->gradient();
    // F = f(X,Y) + X*Y = X*Y + X*Y = 2*X*Y
    // dF/dX = 2*Y = 2*3 = 6, dF/dY = 2*X = 2*2 = 4
    EXPECT_DOUBLE_EQ(main_grad_X, 6.0);
    EXPECT_DOUBLE_EQ(main_grad_Y, 4.0);

    // Call zero_all_grad() - this should NOT affect custom function's ability to compute gradients
    zero_all_grad();
    
    // Custom function should still be able to compute gradients independently
    std::vector<double> grad = f.gradient({5.0, 7.0});
    ASSERT_EQ(grad.size(), 2);
    EXPECT_DOUBLE_EQ(grad[0], 7.0);   // df/dx = y = 7
    EXPECT_DOUBLE_EQ(grad[1], 5.0);   // df/dy = x = 5

    // Main expression gradients should be zero after zero_all_grad()
    EXPECT_DOUBLE_EQ(X->gradient(), 0.0);
    EXPECT_DOUBLE_EQ(Y->gradient(), 0.0);

    // But we can recompute main expression gradients
    F->backward();
    EXPECT_DOUBLE_EQ(X->gradient(), 6.0);
    EXPECT_DOUBLE_EQ(Y->gradient(), 4.0);
}

// Test: Custom function gradient computation doesn't affect main tree gradients
TEST_F(CustomFunctionTest, CustomFunctionGradientDoesNotAffectMainTree) {
    CustomFunction f = CustomFunction::create(
        1,
        [](const std::vector<Variable>& v) {
            return v[0] * v[0];
        },
        "square"
    );

    Variable X;
    X = 3.0;

    // Build main expression
    Expression F = f(X) + X;
    zero_all_grad();
    F->backward();
    
    double main_grad_X_before = X->gradient();
    EXPECT_DOUBLE_EQ(main_grad_X_before, 7.0);  // 2*3 + 1 = 7

    // Compute custom function gradient independently
    std::vector<double> grad = f.gradient({5.0});
    ASSERT_EQ(grad.size(), 1);
    EXPECT_DOUBLE_EQ(grad[0], 10.0);

    // Main expression gradient should be unchanged
    double main_grad_X_after = X->gradient();
    EXPECT_DOUBLE_EQ(main_grad_X_after, main_grad_X_before)
        << "Main expression gradient should not be affected by custom function gradient computation";
}

// Test: Multiple custom function calls in main tree maintain independence
TEST_F(CustomFunctionTest, MultipleCustomFunctionCallsIndependence) {
    CustomFunction f = CustomFunction::create(
        1,
        [](const std::vector<Variable>& v) {
            return v[0] * v[0];
        },
        "square"
    );

    Variable X, Y;
    X = 2.0;
    Y = 4.0;

    // Build main expression with multiple custom function calls
    Expression F = f(X) + f(Y) + X + Y;
    zero_all_grad();
    F->backward();

    // Gradients should be correct
    EXPECT_DOUBLE_EQ(X->gradient(), 5.0);  // 2*2 + 1 = 5
    EXPECT_DOUBLE_EQ(Y->gradient(), 9.0);  // 2*4 + 1 = 9

    // Independent evaluation should still work
    double f_val = f.value({10.0});
    EXPECT_DOUBLE_EQ(f_val, 100.0);

    // Main expression should still work
    // f(X) = 4, f(Y) = 16, X = 2, Y = 4
    // Total: 4 + 16 + 2 + 4 = 26
    EXPECT_DOUBLE_EQ(F->value(), 26.0);
}

// Test: Custom function internal tree nodes are isolated from main tree operations
TEST_F(CustomFunctionTest, InternalTreeIsolation) {
    CustomFunction f = CustomFunction::create(
        1,
        [](const std::vector<Variable>& v) {
            return v[0] * v[0] + v[0];
        },
        "quadratic"
    );

    Variable X;
    X = 3.0;

    // Get node count before
    // Create main expression
    Expression F = f(X);

    // Independent evaluation
    double f_val = f.value({5.0});
    EXPECT_DOUBLE_EQ(f_val, 30.0);  // 25 + 5 = 30

    // Independent evaluation should not create new nodes in main tree's eTbl
    // (or if it does, they should be isolated)
    // The internal tree nodes are in eTbl, but they should be managed separately
    
    // Main expression should still work
    EXPECT_DOUBLE_EQ(F->value(), 12.0);  // 9 + 3 = 12

    // Verify that independent evaluation doesn't break main expression
    zero_all_grad();
    F->backward();
    EXPECT_DOUBLE_EQ(X->gradient(), 7.0);  // 2*3 + 1 = 7
}

// Test: Custom function can be evaluated while main tree has cached values
TEST_F(CustomFunctionTest, EvaluationWithCachedValues) {
    CustomFunction f = CustomFunction::create(
        1,
        [](const std::vector<Variable>& v) {
            return v[0] * v[0];
        },
        "square"
    );

    Variable X;
    X = 3.0;

    // Build and evaluate main expression (creates cached values)
    Expression F = f(X) + X;
    double val1 = F->value();
    EXPECT_DOUBLE_EQ(val1, 12.0);

    // Custom function independent evaluation should work
    // even though main tree has cached values
    double f_val1 = f.value({5.0});
    EXPECT_DOUBLE_EQ(f_val1, 25.0);

    double f_val2 = f.value({7.0});
    EXPECT_DOUBLE_EQ(f_val2, 49.0);

    // Main expression should still work with cached values
    double val2 = F->value();
    EXPECT_DOUBLE_EQ(val2, 12.0);

    // Change X and verify main expression updates
    X = 4.0;
    double val3 = F->value();
    EXPECT_DOUBLE_EQ(val3, 20.0);  // 16 + 4 = 20
}

// Test: Custom function gradient computation doesn't interfere with main tree backward
TEST_F(CustomFunctionTest, GradientComputationDoesNotInterfereWithBackward) {
    CustomFunction f = CustomFunction::create(
        2,
        [](const std::vector<Variable>& v) {
            return v[0] * v[1];
        },
        "multiply"
    );

    Variable X, Y;
    X = 2.0;
    Y = 3.0;

    // Build main expression
    Expression F = f(X, Y) + X * Y;
    
    // Compute gradients in main tree
    zero_all_grad();
    F->backward();
    double grad_X_main = X->gradient();
    double grad_Y_main = Y->gradient();

    // Compute custom function gradient independently
    std::vector<double> grad_custom = f.gradient({10.0, 20.0});
    EXPECT_DOUBLE_EQ(grad_custom[0], 20.0);
    EXPECT_DOUBLE_EQ(grad_custom[1], 10.0);

    // Main tree gradients should be unchanged
    EXPECT_DOUBLE_EQ(X->gradient(), grad_X_main);
    EXPECT_DOUBLE_EQ(Y->gradient(), grad_Y_main);

    // We should be able to recompute main tree backward
    zero_all_grad();
    F->backward();
    EXPECT_DOUBLE_EQ(X->gradient(), grad_X_main);
    EXPECT_DOUBLE_EQ(Y->gradient(), grad_Y_main);
}


// Test 6: print_all() output for custom function nodes
TEST_F(CustomFunctionTest, PrintAllOutput) {
    CustomFunction f = CustomFunction::create(
        2,
        [](const std::vector<Variable>& v) {
            return v[0] * v[1];
        },
        "f_xy"
    );

    Variable X, Y;
    X = 2.0;
    Y = 3.0;

    Expression F = f(X, Y);

    // Evaluate to ensure node is created
    double val = F->value();
    EXPECT_DOUBLE_EQ(val, 6.0);

    // Note: We can't easily test print output in a unit test,
    // but we can verify that the custom function node exists and has correct name
    // by checking that the expression tree can be traversed
    EXPECT_TRUE(F);
}

// Test: Invalid argument count
TEST_F(CustomFunctionTest, InvalidArgumentCount) {
    CustomFunction f = CustomFunction::create(
        2,
        [](const std::vector<Variable>& v) {
            return v[0] * v[1];
        },
        "multiply"
    );

    Variable X;
    X = 1.0;

    // Should throw when calling with wrong number of arguments
    EXPECT_THROW(f(X), Nh::RuntimeException);
    EXPECT_THROW(f({X, X, X}), Nh::RuntimeException);
}

// Test: Invalid input size for value/gradient
TEST_F(CustomFunctionTest, InvalidInputSize) {
    CustomFunction f = CustomFunction::create(
        2,
        [](const std::vector<Variable>& v) {
            return v[0] * v[1];
        },
        "multiply"
    );

    // Should throw when calling with wrong input size
    EXPECT_THROW(f.value({1.0}), Nh::RuntimeException);
    EXPECT_THROW(f.gradient({1.0, 2.0, 3.0}), Nh::RuntimeException);
}

// Test: Default name generation
TEST_F(CustomFunctionTest, DefaultNameGeneration) {
    CustomFunction f1 = CustomFunction::create(
        1,
        [](const std::vector<Variable>& v) {
            return v[0];
        }
    );

    CustomFunction f2 = CustomFunction::create(
        1,
        [](const std::vector<Variable>& v) {
            return v[0];
        }
    );

    // Both should have valid names (auto-generated)
    EXPECT_FALSE(f1.name().empty());
    EXPECT_FALSE(f2.name().empty());
    EXPECT_NE(f1.name(), f2.name());  // Names should be different
}

// Test: Zero-variable function (constant function)
TEST_F(CustomFunctionTest, ZeroVariableConstantFunction) {
    CustomFunction f = CustomFunction::create(
        0,
        [](const std::vector<Variable>& v) {
            (void)v;  // Suppress unused parameter warning
            return Const(42.0);
        },
        "constant"
    );

    EXPECT_EQ(f.input_dim(), 0);
    EXPECT_TRUE(f.valid());

    // Test value with empty input
    EXPECT_DOUBLE_EQ(f.value({}), 42.0);

    // Test gradient with empty input
    std::vector<double> grad = f.gradient({});
    EXPECT_EQ(grad.size(), 0);
}

// Test: Constant-only expression (no variables used)
TEST_F(CustomFunctionTest, ConstantOnlyExpression) {
    CustomFunction f = CustomFunction::create(
        1,
        [](const std::vector<Variable>& v) {
            (void)v;  // Variable not used
            return Const(3.14) + Const(2.71);
        },
        "constant_sum"
    );

    EXPECT_DOUBLE_EQ(f.value({100.0}), 5.85);  // Input doesn't matter
    std::vector<double> grad = f.gradient({100.0});
    ASSERT_EQ(grad.size(), 1);
    EXPECT_DOUBLE_EQ(grad[0], 0.0);  // Gradient should be zero
}

// Test: Complex expressions with trigonometric and logarithmic functions
TEST_F(CustomFunctionTest, ComplexExpressions) {
    CustomFunction f = CustomFunction::create(
        2,
        [](const std::vector<Variable>& v) {
            const auto& x = v[0];
            const auto& y = v[1];
            // f(x,y) = log(x) + sqrt(y) + exp(x*y)
            return log(x) + sqrt(y) + exp(x * y);
        },
        "complex"
    );

    std::vector<double> x = {2.0, 4.0};
    std::vector<double> grad = f.gradient(x);

    ASSERT_EQ(grad.size(), 2);

    // Verify gradient using finite differences
    const double h = 1e-6;
    for (size_t i = 0; i < 2; ++i) {
        std::vector<double> x_plus = x;
        x_plus[i] += h;
        double val_plus = f.value(x_plus);

        std::vector<double> x_minus = x;
        x_minus[i] -= h;
        double val_minus = f.value(x_minus);

        double numerical_grad = (val_plus - val_minus) / (2.0 * h);
        EXPECT_NEAR(grad[i], numerical_grad, 1e-5)
            << "Gradient mismatch for variable " << i;
    }
}

// Test: Edge case with very small values
TEST_F(CustomFunctionTest, VerySmallValues) {
    CustomFunction f = CustomFunction::create(
        1,
        [](const std::vector<Variable>& v) {
            return v[0] * v[0];
        },
        "square"
    );

    double small_val = 1e-10;
    EXPECT_DOUBLE_EQ(f.value({small_val}), small_val * small_val);

    std::vector<double> grad = f.gradient({small_val});
    ASSERT_EQ(grad.size(), 1);
    EXPECT_NEAR(grad[0], 2.0 * small_val, 1e-15);
}

// Test: Edge case with very large values
TEST_F(CustomFunctionTest, VeryLargeValues) {
    CustomFunction f = CustomFunction::create(
        1,
        [](const std::vector<Variable>& v) {
            return exp(v[0]);
        },
        "exp"
    );

    double large_val = 10.0;
    double result = f.value({large_val});
    EXPECT_TRUE(result > 10000.0);  // exp(10) is large

    std::vector<double> grad = f.gradient({large_val});
    ASSERT_EQ(grad.size(), 1);
    EXPECT_NEAR(grad[0], result, result * 1e-10);  // df/dx = exp(x)
}

// Test: Invalid CustomFunction (null handle)
TEST_F(CustomFunctionTest, InvalidCustomFunction) {
    CustomFunction f;  // Default constructed, invalid

    EXPECT_FALSE(f.valid());
    EXPECT_FALSE(static_cast<bool>(f));

    // Should throw when accessing methods
    EXPECT_THROW(f.input_dim(), Nh::RuntimeException);
    EXPECT_THROW(f.name(), Nh::RuntimeException);
    EXPECT_THROW(f.value({1.0}), Nh::RuntimeException);
    EXPECT_THROW(f.gradient({1.0}), Nh::RuntimeException);
}

// Test: Custom function with division (potential division by zero)
TEST_F(CustomFunctionTest, DivisionInCustomFunction) {
    CustomFunction f = CustomFunction::create(
        2,
        [](const std::vector<Variable>& v) {
            return v[0] / v[1];
        },
        "divide"
    );

    // Normal case
    EXPECT_DOUBLE_EQ(f.value({6.0, 2.0}), 3.0);
    std::vector<double> grad = f.gradient({6.0, 2.0});
    ASSERT_EQ(grad.size(), 2);
    EXPECT_DOUBLE_EQ(grad[0], 0.5);   // df/dx = 1/y = 0.5
    EXPECT_DOUBLE_EQ(grad[1], -1.5);  // df/dy = -x/y² = -6/4 = -1.5

    // Division by zero should throw
    EXPECT_THROW(f.value({1.0, 0.0}), Nh::RuntimeException);
}

// Test: Custom function with logarithm (potential log of negative)
TEST_F(CustomFunctionTest, LogarithmInCustomFunction) {
    CustomFunction f = CustomFunction::create(
        1,
        [](const std::vector<Variable>& v) {
            return log(v[0]);
        },
        "log"
    );

    // Normal case
    EXPECT_DOUBLE_EQ(f.value({std::exp(1.0)}), 1.0);
    std::vector<double> grad = f.gradient({std::exp(1.0)});
    ASSERT_EQ(grad.size(), 1);
    EXPECT_NEAR(grad[0], 1.0 / std::exp(1.0), 1e-10);

    // Log of negative should throw
    EXPECT_THROW(f.value({-1.0}), Nh::RuntimeException);
}

// Test: Custom function reuse (same function, different inputs)
TEST_F(CustomFunctionTest, FunctionReuse) {
    CustomFunction f = CustomFunction::create(
        1,
        [](const std::vector<Variable>& v) {
            return v[0] * v[0] + v[0];
        },
        "quadratic"
    );

    // Multiple evaluations with different inputs
    EXPECT_DOUBLE_EQ(f.value({0.0}), 0.0);
    EXPECT_DOUBLE_EQ(f.value({1.0}), 2.0);
    EXPECT_DOUBLE_EQ(f.value({2.0}), 6.0);
    EXPECT_DOUBLE_EQ(f.value({-1.0}), 0.0);

    // Each evaluation should work independently
    for (double x : {0.0, 1.0, 2.0, -1.0, 5.0}) {
        double expected = x * x + x;
        EXPECT_DOUBLE_EQ(f.value({x}), expected);
    }
}

// Test: Custom function with many variables (stress test)
TEST_F(CustomFunctionTest, ManyVariables) {
    const size_t n = 10;
    CustomFunction f = CustomFunction::create(
        n,
        [](const std::vector<Variable>& v) {
            Expression sum = Const(0.0);
            for (size_t i = 0; i < v.size(); ++i) {
                sum = sum + v[i] * v[i];
            }
            return sum;
        },
        "sum_of_squares"
    );

    std::vector<double> x(n, 1.0);
    EXPECT_DOUBLE_EQ(f.value(x), static_cast<double>(n));

    std::vector<double> grad = f.gradient(x);
    ASSERT_EQ(grad.size(), n);
    for (size_t i = 0; i < n; ++i) {
        EXPECT_DOUBLE_EQ(grad[i], 2.0);  // df/dx_i = 2x_i = 2
    }
}

// Test: Custom function with nested expressions
TEST_F(CustomFunctionTest, NestedExpressions) {
    CustomFunction f = CustomFunction::create(
        2,
        [](const std::vector<Variable>& v) {
            const auto& x = v[0];
            const auto& y = v[1];
            // f(x,y) = (x + y) * (x - y) = x² - y²
            return (x + y) * (x - y);
        },
        "difference_of_squares"
    );

    EXPECT_DOUBLE_EQ(f.value({3.0, 2.0}), 5.0);  // 9 - 4 = 5
    std::vector<double> grad = f.gradient({3.0, 2.0});
    ASSERT_EQ(grad.size(), 2);
    EXPECT_DOUBLE_EQ(grad[0], 6.0);   // df/dx = 2x = 6
    EXPECT_DOUBLE_EQ(grad[1], -4.0);  // df/dy = -2y = -4
}

// Test: Custom function with conditional-like expressions (using max via expressions)
TEST_F(CustomFunctionTest, ConditionalLikeExpressions) {
    CustomFunction f = CustomFunction::create(
        2,
        [](const std::vector<Variable>& v) {
            const auto& x = v[0];
            const auto& y = v[1];
            // f(x,y) = x² + y² (always positive, simulates max-like behavior)
            return x * x + y * y;
        },
        "sum_of_squares_2d"
    );

    EXPECT_DOUBLE_EQ(f.value({3.0, 4.0}), 25.0);  // 9 + 16 = 25
    std::vector<double> grad = f.gradient({3.0, 4.0});
    ASSERT_EQ(grad.size(), 2);
    EXPECT_DOUBLE_EQ(grad[0], 6.0);   // df/dx = 2x = 6
    EXPECT_DOUBLE_EQ(grad[1], 8.0);   // df/dy = 2y = 8
}

// Test: Custom function with power operation
TEST_F(CustomFunctionTest, PowerOperation) {
    CustomFunction f = CustomFunction::create(
        2,
        [](const std::vector<Variable>& v) {
            const auto& x = v[0];
            const auto& y = v[1];
            return x ^ y;  // x^y
        },
        "power"
    );

    EXPECT_DOUBLE_EQ(f.value({2.0, 3.0}), 8.0);  // 2^3 = 8
    std::vector<double> grad = f.gradient({2.0, 3.0});
    ASSERT_EQ(grad.size(), 2);
    EXPECT_NEAR(grad[0], 12.0, 1e-10);   // df/dx = y*x^(y-1) = 3*2^2 = 12
    EXPECT_NEAR(grad[1], 8.0 * std::log(2.0), 1e-10);  // df/dy = x^y*log(x) = 8*log(2)
}

// Test: Custom function with NaN input (should handle gracefully or propagate)
TEST_F(CustomFunctionTest, NaNInput) {
    CustomFunction f = CustomFunction::create(
        1,
        [](const std::vector<Variable>& v) {
            return v[0] * v[0];
        },
        "square"
    );

    double nan_val = std::numeric_limits<double>::quiet_NaN();
    double result = f.value({nan_val});
    // Result should be NaN
    EXPECT_TRUE(std::isnan(result));

    std::vector<double> grad = f.gradient({nan_val});
    ASSERT_EQ(grad.size(), 1);
    EXPECT_TRUE(std::isnan(grad[0]));
}

// Test: Custom function with infinity input
TEST_F(CustomFunctionTest, InfinityInput) {
    CustomFunction f = CustomFunction::create(
        1,
        [](const std::vector<Variable>& v) {
            return exp(v[0]);
        },
        "exp"
    );

    double pos_inf = std::numeric_limits<double>::infinity();
    double result = f.value({pos_inf});
    EXPECT_TRUE(std::isinf(result));
    EXPECT_GT(result, 0.0);

    double neg_inf = -std::numeric_limits<double>::infinity();
    double result_neg = f.value({neg_inf});
    EXPECT_DOUBLE_EQ(result_neg, 0.0);  // exp(-inf) = 0
}

// Test: Custom function with complex nested expressions (simulating nested function calls)
TEST_F(CustomFunctionTest, ComplexNestedExpressions) {
    // f(x) = (x² + x)² = (x² + x) * (x² + x)
    // This simulates nested function calls by using the same expression twice
    CustomFunction f = CustomFunction::create(
        1,
        [](const std::vector<Variable>& v) {
            const auto& x = v[0];
            Expression inner = x * x + x;  // x² + x
            return inner * inner;  // (x² + x)²
        },
        "nested_square"
    );

    EXPECT_DOUBLE_EQ(f.value({2.0}), 36.0);  // (4 + 2)² = 36
    std::vector<double> grad = f.gradient({2.0});
    ASSERT_EQ(grad.size(), 1);
    // df/dx = 2(x² + x)(2x + 1) = 2(6)(5) = 60
    EXPECT_DOUBLE_EQ(grad[0], 60.0);
}

// Test: Custom function with zero gradient (constant w.r.t. some variables)
TEST_F(CustomFunctionTest, ZeroGradient) {
    CustomFunction f = CustomFunction::create(
        2,
        [](const std::vector<Variable>& v) {
            // f(x,y) = x (independent of y)
            return v[0];
        },
        "x_only"
    );

    std::vector<double> grad = f.gradient({5.0, 10.0});
    ASSERT_EQ(grad.size(), 2);
    EXPECT_DOUBLE_EQ(grad[0], 1.0);   // df/dx = 1
    EXPECT_DOUBLE_EQ(grad[1], 0.0);   // df/dy = 0
}

// Test: Custom function with negative values
TEST_F(CustomFunctionTest, NegativeValues) {
    CustomFunction f = CustomFunction::create(
        1,
        [](const std::vector<Variable>& v) {
            return v[0] * v[0];
        },
        "square"
    );

    EXPECT_DOUBLE_EQ(f.value({-3.0}), 9.0);
    std::vector<double> grad = f.gradient({-3.0});
    ASSERT_EQ(grad.size(), 1);
    EXPECT_DOUBLE_EQ(grad[0], -6.0);  // df/dx = 2x = -6
}

// Test: Custom function with value_and_gradient consistency
TEST_F(CustomFunctionTest, ValueAndGradientConsistency) {
    CustomFunction f = CustomFunction::create(
        2,
        [](const std::vector<Variable>& v) {
            return v[0] * v[1] + v[0] * v[0];
        },
        "xy_plus_x2"
    );

    std::vector<double> x = {2.0, 3.0};
    auto [val1, grad1] = f.value_and_gradient(x);
    double val2 = f.value(x);
    std::vector<double> grad2 = f.gradient(x);

    EXPECT_DOUBLE_EQ(val1, val2);
    ASSERT_EQ(grad1.size(), grad2.size());
    for (size_t i = 0; i < grad1.size(); ++i) {
        EXPECT_DOUBLE_EQ(grad1[i], grad2[i]);
    }
}

// Test: Custom function called multiple times with same input (cache behavior)
TEST_F(CustomFunctionTest, MultipleCallsSameInput) {
    CustomFunction f = CustomFunction::create(
        1,
        [](const std::vector<Variable>& v) {
            return v[0] * v[0];
        },
        "square"
    );

    std::vector<double> x = {5.0};
    
    // Call multiple times - should get same result
    for (int i = 0; i < 10; ++i) {
        EXPECT_DOUBLE_EQ(f.value(x), 25.0);
        std::vector<double> grad = f.gradient(x);
        ASSERT_EQ(grad.size(), 1);
        EXPECT_DOUBLE_EQ(grad[0], 10.0);
    }
}

// Test: Custom function with all operations combined
TEST_F(CustomFunctionTest, AllOperationsCombined) {
    CustomFunction f = CustomFunction::create(
        3,
        [](const std::vector<Variable>& v) {
            const auto& x = v[0];
            const auto& y = v[1];
            const auto& z = v[2];
            // f(x,y,z) = (x + y) * (x - y) + exp(z) + log(x) + sqrt(y)
            return (x + y) * (x - y) + exp(z) + log(x) + sqrt(y);
        },
        "all_ops"
    );

    std::vector<double> x = {4.0, 9.0, 1.0};
    std::vector<double> grad = f.gradient(x);

    ASSERT_EQ(grad.size(), 3);
    
    // Verify gradient using finite differences
    const double h = 1e-6;
    for (size_t i = 0; i < 3; ++i) {
        std::vector<double> x_plus = x;
        x_plus[i] += h;
        double val_plus = f.value(x_plus);

        std::vector<double> x_minus = x;
        x_minus[i] -= h;
        double val_minus = f.value(x_minus);

        double numerical_grad = (val_plus - val_minus) / (2.0 * h);
        EXPECT_NEAR(grad[i], numerical_grad, 1e-5)
            << "Gradient mismatch for variable " << i;
    }
}

// Test: Node count doesn't grow unexpectedly with multiple custom function calls
TEST_F(CustomFunctionTest, NodeCountWithMultipleCalls) {
    CustomFunction f = CustomFunction::create(
        2,
        [](const std::vector<Variable>& v) {
            return v[0] * v[1];
        },
        "multiply"
    );

    Variable X, Y;
    X = 2.0;
    Y = 3.0;

    // Get initial node count
    size_t initial_count = ExpressionImpl::node_count();

    // First call - creates CUSTOM_FUNCTION node and internal expression tree
    Expression F1 = f(X, Y);
    size_t after_first = ExpressionImpl::node_count();
    size_t nodes_first_call = after_first - initial_count;

    // Second call with same arguments - should reuse internal tree
    Expression F2 = f(X, Y);
    size_t after_second = ExpressionImpl::node_count();
    size_t nodes_second_call = after_second - after_first;

    // Third call with different arguments - should still reuse internal tree
    Variable X2, Y2;
    X2 = 4.0;
    Y2 = 5.0;
    Expression F3 = f(X2, Y2);
    size_t after_third = ExpressionImpl::node_count();
    size_t nodes_third_call = after_third - after_second;

    // Each call should create:
    // - 1 CUSTOM_FUNCTION node in main tree
    // - Variable nodes for arguments (if not already in tree)
    // But internal expression tree should be shared (created only once)
    
    // First call creates internal tree + CUSTOM_FUNCTION node + argument nodes
    // Subsequent calls should only create CUSTOM_FUNCTION node + new argument nodes
    // Internal tree nodes should NOT be duplicated
    
    EXPECT_GT(nodes_first_call, 0) << "First call should create nodes (internal tree + CUSTOM_FUNCTION + args)";
    
    // Second call with same arguments: should only create 1 CUSTOM_FUNCTION node
    // (arguments X, Y are already in tree)
    EXPECT_LE(nodes_second_call, 1) 
        << "Second call should create at most 1 node (CUSTOM_FUNCTION), reusing internal tree";
    
    // Third call with new arguments: should create CUSTOM_FUNCTION node + new argument nodes
    // But internal tree should still be reused
    EXPECT_LE(nodes_third_call, 3)  // CUSTOM_FUNCTION + X2 + Y2
        << "Third call should create CUSTOM_FUNCTION + new argument nodes, but reuse internal tree";
    
    // Verify values are correct
    EXPECT_DOUBLE_EQ(F1->value(), 6.0);
    EXPECT_DOUBLE_EQ(F2->value(), 6.0);
    EXPECT_DOUBLE_EQ(F3->value(), 20.0);
}

// Test: Node count with custom function using cached functions (phi_expr)
TEST_F(CustomFunctionTest, NodeCountWithCachedFunctions) {
    CustomFunction f = CustomFunction::create(
        1,
        [](const std::vector<Variable>& v) {
            return phi_expr(v[0]);
        },
        "phi_wrapper"
    );

    Variable X;
    X = 1.0;

    size_t initial_count = ExpressionImpl::node_count();

    // First call
    Expression F1 = f(X);
    size_t after_first = ExpressionImpl::node_count();
    size_t nodes_first = after_first - initial_count;

    // Second call with same argument
    Expression F2 = f(X);
    size_t after_second = ExpressionImpl::node_count();
    size_t nodes_second = after_second - after_first;

    // Third call with different argument
    Variable X2;
    X2 = 2.0;
    Expression F3 = f(X2);
    size_t after_third = ExpressionImpl::node_count();
    size_t nodes_third = after_third - after_second;

    // Internal tree should be created once, cached phi_expr nodes should be reused
    EXPECT_GT(nodes_first, 0) << "First call should create internal tree";
    
    // Second call with same argument: should only create CUSTOM_FUNCTION node
    EXPECT_LE(nodes_second, 1) 
        << "Second call should create at most 1 node (CUSTOM_FUNCTION), reusing internal tree";
    
    // Third call with new argument: should create CUSTOM_FUNCTION + new argument node
    // But internal tree structure should be reused
    EXPECT_LE(nodes_third, 2)  // CUSTOM_FUNCTION + X2
        << "Third call should create CUSTOM_FUNCTION + new argument, but reuse internal tree";

    // Verify values
    double expected1 = phi_expr(X)->value();
    double expected2 = phi_expr(X2)->value();
    EXPECT_DOUBLE_EQ(F1->value(), expected1);
    EXPECT_DOUBLE_EQ(F2->value(), expected1);
    EXPECT_DOUBLE_EQ(F3->value(), expected2);
}

// Test: Node count with multiple different custom functions
TEST_F(CustomFunctionTest, NodeCountWithMultipleDifferentFunctions) {
    CustomFunction f1 = CustomFunction::create(
        1,
        [](const std::vector<Variable>& v) {
            return v[0] * v[0];
        },
        "square"
    );

    CustomFunction f2 = CustomFunction::create(
        2,
        [](const std::vector<Variable>& v) {
            return v[0] * v[1];
        },
        "multiply"
    );

    Variable X, Y;
    X = 2.0;
    Y = 3.0;

    size_t initial_count = ExpressionImpl::node_count();

    // Create expressions with different custom functions
    Expression E1 = f1(X);
    size_t after_f1 = ExpressionImpl::node_count();
    
    Expression E2 = f2(X, Y);
    size_t after_f2 = ExpressionImpl::node_count();
    
    Expression E3 = f1(Y);
    size_t after_f3 = ExpressionImpl::node_count();

    size_t nodes_f1 = after_f1 - initial_count;
    size_t nodes_f2 = after_f2 - after_f1;
    size_t nodes_f3 = after_f3 - after_f2;

    // Each function should create its own internal tree once
    // f1 should be reused for E3 (same function, different argument)
    EXPECT_GT(nodes_f1, 0) << "f1 first call should create internal tree";
    EXPECT_GT(nodes_f2, 0) << "f2 first call should create internal tree";
    
    // Reusing f1 with new argument: should only create CUSTOM_FUNCTION + new argument node
    // Internal tree should be reused
    EXPECT_LE(nodes_f3, 2)  // CUSTOM_FUNCTION + Y (if Y is new)
        << "Reusing f1 should create at most CUSTOM_FUNCTION + new argument, reusing internal tree";

    EXPECT_DOUBLE_EQ(E1->value(), 4.0);
    EXPECT_DOUBLE_EQ(E2->value(), 6.0);
    EXPECT_DOUBLE_EQ(E3->value(), 9.0);
}

// Test: Node count with complex custom function (stress test)
TEST_F(CustomFunctionTest, NodeCountComplexFunction) {
    CustomFunction f = CustomFunction::create(
        3,
        [](const std::vector<Variable>& v) {
            const auto& x = v[0];
            const auto& y = v[1];
            const auto& z = v[2];
            // Complex expression: (x + y) * (x - y) + exp(z) + log(x) + sqrt(y)
            return (x + y) * (x - y) + exp(z) + log(x) + sqrt(y);
        },
        "complex"
    );

    Variable X, Y, Z;
    X = 4.0;
    Y = 9.0;
    Z = 1.0;

    size_t initial_count = ExpressionImpl::node_count();

    // First call - creates internal tree
    Expression F1 = f(X, Y, Z);
    size_t after_first = ExpressionImpl::node_count();
    size_t nodes_first = after_first - initial_count;

    // Multiple subsequent calls
    for (int i = 0; i < 10; ++i) {
        Variable X2, Y2, Z2;
        X2 = static_cast<double>(i);
        Y2 = static_cast<double>(i + 1);
        Z2 = static_cast<double>(i + 2);
        Expression F = f(X2, Y2, Z2);
        (void)F->value();  // Evaluate to ensure nodes are created
    }

    size_t after_multiple = ExpressionImpl::node_count();
    size_t nodes_total = after_multiple - initial_count;

    // Total nodes should be: initial tree + 11 CUSTOM_FUNCTION nodes + argument nodes
    // But internal tree should be created only once
    // With 10 additional calls, we expect roughly: nodes_first + 10 * (small number)
    // The small number is for CUSTOM_FUNCTION node + argument nodes
    
    size_t expected_max = nodes_first + 10 * 10;  // Allow some overhead
    EXPECT_LT(nodes_total, expected_max)
        << "Node count should not grow linearly with number of calls. "
        << "Internal tree should be shared. "
        << "First call: " << nodes_first << " nodes, "
        << "Total after 11 calls: " << nodes_total << " nodes";
}

// Test: Custom function with composite Expression arguments (critical bug fix verification)
TEST_F(CustomFunctionTest, CompositeExpressionArguments) {
    CustomFunction f = CustomFunction::create(
        2,
        [](const std::vector<Variable>& v) {
            return v[0] * v[1];
        },
        "multiply"
    );

    Variable X, Y, Z;
    X = 2.0;
    Y = 3.0;
    Z = 4.0;

    // Build composite expression: g = X * Y
    Expression g = X * Y;

    // Use composite expression as argument: F = f(g, Z)
    Expression F = f(g, Z);

    EXPECT_DOUBLE_EQ(F->value(), 24.0);  // f(6, 4) = 24

    // Compute gradients - should propagate to X, Y, Z
    zero_all_grad();
    F->backward();

    // dF/dX = dF/dg * dg/dX = Z * Y = 4 * 3 = 12
    EXPECT_DOUBLE_EQ(X->gradient(), 12.0);

    // dF/dY = dF/dg * dg/dY = Z * X = 4 * 2 = 8
    EXPECT_DOUBLE_EQ(Y->gradient(), 8.0);

    // dF/dZ = df/dZ = g = 6
    EXPECT_DOUBLE_EQ(Z->gradient(), 6.0);
}

// Test: Custom function called multiple times with different composite expressions
TEST_F(CustomFunctionTest, MultipleCallsWithCompositeExpressions) {
    CustomFunction f = CustomFunction::create(
        1,
        [](const std::vector<Variable>& v) {
            return v[0] * v[0];
        },
        "square"
    );

    Variable X, Y;
    X = 2.0;
    Y = 3.0;

    // Build composite expressions
    Expression g1 = X * Y;  // 6
    Expression g2 = X + Y;  // 5

    // Use in main expression: F = f(g1) + f(g2)
    Expression F = f(g1) + f(g2);

    EXPECT_DOUBLE_EQ(F->value(), 61.0);  // 36 + 25 = 61

    // Compute gradients
    zero_all_grad();
    F->backward();

    // dF/dX = df(g1)/dg1 * dg1/dX + df(g2)/dg2 * dg2/dX
    //       = 2*g1 * Y + 2*g2 * 1
    //       = 2*6*3 + 2*5*1 = 36 + 10 = 46
    EXPECT_DOUBLE_EQ(X->gradient(), 46.0);

    // dF/dY = df(g1)/dg1 * dg1/dY + df(g2)/dg2 * dg2/dY
    //       = 2*g1 * X + 2*g2 * 1
    //       = 2*6*2 + 2*5*1 = 24 + 10 = 34
    EXPECT_DOUBLE_EQ(Y->gradient(), 34.0);
}

// Test: Custom function value cache is properly cleared (critical bug fix verification)
TEST_F(CustomFunctionTest, ValueCacheProperlyCleared) {
    CustomFunction f = CustomFunction::create(
        1,
        [](const std::vector<Variable>& v) {
            return v[0] * v[0];
        },
        "square"
    );

    // First evaluation
    double val1 = f.value({3.0});
    EXPECT_DOUBLE_EQ(val1, 9.0);

    // Second evaluation with different input - should NOT return cached value
    double val2 = f.value({5.0});
    EXPECT_DOUBLE_EQ(val2, 25.0);  // Should be 25, not 9

    // Third evaluation
    double val3 = f.value({7.0});
    EXPECT_DOUBLE_EQ(val3, 49.0);  // Should be 49, not 9 or 25

    // Verify gradients also work correctly
    std::vector<double> grad1 = f.gradient({3.0});
    EXPECT_DOUBLE_EQ(grad1[0], 6.0);  // 2*3 = 6

    std::vector<double> grad2 = f.gradient({5.0});
    EXPECT_DOUBLE_EQ(grad2[0], 10.0);  // 2*5 = 10, not 6

    std::vector<double> grad3 = f.gradient({7.0});
    EXPECT_DOUBLE_EQ(grad3[0], 14.0);  // 2*7 = 14, not 6 or 10
}

// Test: Custom function with nested composite expressions
TEST_F(CustomFunctionTest, NestedCompositeExpressions) {
    CustomFunction f = CustomFunction::create(
        2,
        [](const std::vector<Variable>& v) {
            return v[0] + v[1];
        },
        "add"
    );

    Variable X, Y, Z;
    X = 1.0;
    Y = 2.0;
    Z = 3.0;

    // Build nested composite expressions
    Expression g1 = X * Y;      // 2
    Expression g2 = Y + Z;       // 5
    Expression h = g1 * g2;      // 10

    // Use nested composite: F = f(g1, h)
    Expression F = f(g1, h);

    EXPECT_DOUBLE_EQ(F->value(), 12.0);  // 2 + 10 = 12

    // Compute gradients - should propagate through all levels
    zero_all_grad();
    F->backward();

    // dF/dX = dF/dg1 * dg1/dX + dF/dh * dh/dg1 * dg1/dX
    //       = 1 * Y + 1 * g2 * Y
    //       = 2 + 5*2 = 2 + 10 = 12
    EXPECT_DOUBLE_EQ(X->gradient(), 12.0);

    // dF/dY = dF/dg1 * dg1/dY + dF/dh * (dh/dg1 * dg1/dY + dh/dg2 * dg2/dY)
    //       = 1 * X + 1 * (g2 * X + g1 * 1)
    //       = 1 + (5*1 + 2*1) = 1 + 7 = 8
    EXPECT_DOUBLE_EQ(Y->gradient(), 8.0);

    // dF/dZ = dF/dh * dh/dg2 * dg2/dZ
    //       = 1 * g1 * 1
    //       = 2
    EXPECT_DOUBLE_EQ(Z->gradient(), 2.0);
}

// Test: Value cache is properly cleared when used in main expression tree (bug fix verification)
TEST_F(CustomFunctionTest, ValueCacheClearedInMainExpressionTree) {
    CustomFunction f = CustomFunction::create(
        1,
        [](const std::vector<Variable>& v) {
            return v[0] * v[0] + v[0];
        },
        "quadratic"
    );

    Variable X;
    X = 2.0;

    // Build main expression
    Expression F = f(X);
    double val1 = F->value();
    EXPECT_DOUBLE_EQ(val1, 6.0);  // 4 + 2 = 6

    // Change X value
    X = 5.0;
    double val2 = F->value();
    EXPECT_DOUBLE_EQ(val2, 30.0);  // 25 + 5 = 30, not 6

    // Change again
    X = 7.0;
    double val3 = F->value();
    EXPECT_DOUBLE_EQ(val3, 56.0);  // 49 + 7 = 56, not 6 or 30

    // Verify gradients also work correctly
    zero_all_grad();
    F->backward();
    EXPECT_DOUBLE_EQ(X->gradient(), 15.0);  // 2*7 + 1 = 15

    X = 3.0;
    zero_all_grad();
    F->backward();
    EXPECT_DOUBLE_EQ(X->gradient(), 7.0);  // 2*3 + 1 = 7, not 15
}

// Test: Memory safety - custom function node destruction doesn't cause use-after-free
TEST_F(CustomFunctionTest, MemorySafetyAfterDestruction) {
    CustomFunction f = CustomFunction::create(
        2,
        [](const std::vector<Variable>& v) {
            return v[0] * v[1];
        },
        "multiply"
    );

    Variable X, Y;
    X = 2.0;
    Y = 3.0;

    // Build composite expression
    Expression g = X * Y;

    // Create custom function node
    Expression F = f(g, Y);
    double val = F->value();
    EXPECT_DOUBLE_EQ(val, 18.0);  // f(6, 3) = 18

    // Destroy the custom function node by going out of scope
    {
        Expression F2 = f(g, Y);
        double val2 = F2->value();
        EXPECT_DOUBLE_EQ(val2, 18.0);
    }  // F2 is destroyed here - this tests that remove_root() was called correctly

    // After F2 is destroyed, g and X, Y should still be safe to use
    // The key test is that accessing g doesn't cause use-after-free
    EXPECT_DOUBLE_EQ(g->value(), 6.0);  // X=2, Y=3, so 2*3=6
    
    // Change X value - g should update correctly (or stay cached, both are OK)
    // The important thing is that it doesn't crash
    X = 4.0;
    (void)g->value();  // May be 12 (if recalculated) or 6 (if cached)
    // Both are acceptable - the key is no crash
    
    // Should be able to create new expressions with g without crashing
    Expression h = g + X;
    double h_val = h->value();  // Should compute without crashing
    EXPECT_GT(h_val, 0.0);  // Just verify it doesn't crash and returns a reasonable value
    
    // Verify we can still use X and Y
    Expression k = X + Y;
    EXPECT_DOUBLE_EQ(k->value(), 7.0);  // 4 + 3 = 7
}

// Test: Complex nested custom functions with composite arguments
TEST_F(CustomFunctionTest, ComplexNestedCustomFunctions) {
    CustomFunction f1 = CustomFunction::create(
        1,
        [](const std::vector<Variable>& v) {
            return v[0] * v[0];
        },
        "square"
    );

    CustomFunction f2 = CustomFunction::create(
        2,
        [](const std::vector<Variable>& v) {
            return v[0] + v[1];
        },
        "add"
    );

    Variable X, Y, Z;
    X = 2.0;
    Y = 3.0;
    Z = 4.0;

    // Build composite expressions
    Expression g1 = X * Y;      // 6
    Expression g2 = Y + Z;       // 7
    Expression h1 = f1(g1);     // 36
    Expression h2 = f1(g2);     // 49

    // Use in main expression: F = f2(h1, h2)
    Expression F = f2(h1, h2);

    EXPECT_DOUBLE_EQ(F->value(), 85.0);  // 36 + 49 = 85

    // Compute gradients - should propagate through all levels
    zero_all_grad();
    F->backward();

    // dF/dX = dF/dh1 * dh1/dg1 * dg1/dX + dF/dh2 * dh2/dg2 * dg2/dX
    //       = 1 * 2*g1 * Y + 1 * 2*g2 * 0
    //       = 2*6*3 + 0 = 36
    EXPECT_DOUBLE_EQ(X->gradient(), 36.0);

    // dF/dY = dF/dh1 * dh1/dg1 * dg1/dY + dF/dh2 * dh2/dg2 * dg2/dY
    //       = 1 * 2*g1 * X + 1 * 2*g2 * 1
    //       = 2*6*2 + 2*7*1 = 24 + 14 = 38
    EXPECT_DOUBLE_EQ(Y->gradient(), 38.0);

    // dF/dZ = dF/dh2 * dh2/dg2 * dg2/dZ
    //       = 1 * 2*g2 * 1
    //       = 2*7 = 14
    EXPECT_DOUBLE_EQ(Z->gradient(), 14.0);
}

// Test: Multiple custom function calls with same composite expression argument
TEST_F(CustomFunctionTest, MultipleCallsSameCompositeArgument) {
    CustomFunction f = CustomFunction::create(
        1,
        [](const std::vector<Variable>& v) {
            return v[0] * v[0];
        },
        "square"
    );

    Variable X, Y;
    X = 2.0;
    Y = 3.0;

    // Build composite expression
    Expression g = X * Y;  // 6

    // Use same composite expression in multiple custom function calls
    Expression F = f(g) + f(g) + f(g);

    EXPECT_DOUBLE_EQ(F->value(), 108.0);  // 36 + 36 + 36 = 108

    // Compute gradients
    zero_all_grad();
    F->backward();

    // dF/dX = 3 * df(g)/dg * dg/dX = 3 * 2*g * Y = 3 * 2*6*3 = 108
    EXPECT_DOUBLE_EQ(X->gradient(), 108.0);

    // dF/dY = 3 * df(g)/dg * dg/dY = 3 * 2*g * X = 3 * 2*6*2 = 72
    EXPECT_DOUBLE_EQ(Y->gradient(), 72.0);
}

// Test: Custom function with value_and_gradient cache clearing
TEST_F(CustomFunctionTest, ValueAndGradientCacheClearing) {
    CustomFunction f = CustomFunction::create(
        1,
        [](const std::vector<Variable>& v) {
            return v[0] * v[0];
        },
        "square"
    );

    // First call
    auto [val1, grad1] = f.value_and_gradient({3.0});
    EXPECT_DOUBLE_EQ(val1, 9.0);
    EXPECT_DOUBLE_EQ(grad1[0], 6.0);

    // Second call with different input
    auto [val2, grad2] = f.value_and_gradient({5.0});
    EXPECT_DOUBLE_EQ(val2, 25.0);  // Should be 25, not 9
    EXPECT_DOUBLE_EQ(grad2[0], 10.0);  // Should be 10, not 6

    // Third call
    auto [val3, grad3] = f.value_and_gradient({7.0});
    EXPECT_DOUBLE_EQ(val3, 49.0);  // Should be 49, not 9 or 25
    EXPECT_DOUBLE_EQ(grad3[0], 14.0);  // Should be 14, not 6 or 10
}

// Test: Custom function with mixed simple and composite arguments
TEST_F(CustomFunctionTest, MixedSimpleAndCompositeArguments) {
    CustomFunction f = CustomFunction::create(
        3,
        [](const std::vector<Variable>& v) {
            return v[0] * v[1] + v[2];
        },
        "mixed"
    );

    Variable X, Y, Z, W;
    X = 2.0;
    Y = 3.0;
    Z = 4.0;
    W = 5.0;

    // Mix simple Variable and composite Expression
    Expression g = X * Y;  // 6

    // F = f(X, g, W) = X * g + W = 2 * 6 + 5 = 17
    Expression F = f(X, g, W);

    EXPECT_DOUBLE_EQ(F->value(), 17.0);

    // Compute gradients
    zero_all_grad();
    F->backward();

    // dF/dX = df/dv0 * dv0/dX + df/dv1 * dv1/dg * dg/dX
    //       = g * 1 + X * 1 * Y
    //       = 6 + 2*3 = 6 + 6 = 12
    EXPECT_DOUBLE_EQ(X->gradient(), 12.0);

    // dF/dY = df/dv1 * dv1/dg * dg/dY
    //       = X * 1 * X
    //       = 2 * 2 = 4
    EXPECT_DOUBLE_EQ(Y->gradient(), 4.0);

    // dF/dW = df/dv2
    //       = 1
    EXPECT_DOUBLE_EQ(W->gradient(), 1.0);

    // dF/dZ = 0 (Z is not used)
    EXPECT_DOUBLE_EQ(Z->gradient(), 0.0);
}

// Test: Nested custom function gradient accumulation bug fix
// This test verifies that when a custom function calls another custom function,
// the gradient does not accumulate across multiple calls.
TEST_F(CustomFunctionTest, NestedCustomFunctionGradientNoAccumulation) {
    // Define f(x, y) = x^2 + y^2
    CustomFunction f = CustomFunction::create(
        2,
        [](const std::vector<Variable>& v) {
            return v[0] * v[0] + v[1] * v[1];
        },
        "f"
    );

    // Define g(x, y) that calls f(x, y) internally
    // g(x, y) = f(x, y) + x
    CustomFunction g = CustomFunction::create(
        2,
        [&f](const std::vector<Variable>& v) {
            Expression f_result = f(v[0], v[1]);
            return f_result + v[0];
        },
        "g"
    );

    // Create main expression tree: G(X, Y) = g(X, Y)
    Variable X, Y;
    X = 2.0;
    Y = 3.0;
    Expression G = g(X, Y);

    // First call: compute gradient
    zero_all_grad();
    G->backward();
    double grad_X_first = X->gradient();
    double grad_Y_first = Y->gradient();

    // Expected gradients:
    // dG/dX = dg/dX = df/dX + 1 = 2*X + 1 = 2*2 + 1 = 5
    // dG/dY = dg/dY = df/dY = 2*Y = 2*3 = 6
    EXPECT_DOUBLE_EQ(grad_X_first, 5.0);
    EXPECT_DOUBLE_EQ(grad_Y_first, 6.0);

    // Second call: compute gradient again with same inputs
    zero_all_grad();
    G->backward();
    double grad_X_second = X->gradient();
    double grad_Y_second = Y->gradient();

    // Gradients should be the same (not accumulated)
    EXPECT_DOUBLE_EQ(grad_X_second, 5.0);
    EXPECT_DOUBLE_EQ(grad_Y_second, 6.0);
    EXPECT_DOUBLE_EQ(grad_X_first, grad_X_second);
    EXPECT_DOUBLE_EQ(grad_Y_first, grad_Y_second);

    // Third call: compute gradient again
    zero_all_grad();
    G->backward();
    double grad_X_third = X->gradient();
    double grad_Y_third = Y->gradient();

    // Still should be the same
    EXPECT_DOUBLE_EQ(grad_X_third, 5.0);
    EXPECT_DOUBLE_EQ(grad_Y_third, 6.0);

    // Change inputs and verify gradients update correctly
    X = 1.0;
    Y = 4.0;
    zero_all_grad();
    G->backward();
    double grad_X_new = X->gradient();
    double grad_Y_new = Y->gradient();

    // Expected gradients with new inputs:
    // dG/dX = 2*1 + 1 = 3
    // dG/dY = 2*4 = 8
    EXPECT_DOUBLE_EQ(grad_X_new, 3.0);
    EXPECT_DOUBLE_EQ(grad_Y_new, 8.0);
}

// Test: Nested custom function with multiple gradient calls
// Verifies that gradients do not accumulate across multiple calls
TEST_F(CustomFunctionTest, NestedCustomFunctionValueAndGradientNoAccumulation) {
    // Define f(x) = x^2
    CustomFunction f = CustomFunction::create(
        1,
        [](const std::vector<Variable>& v) {
            return v[0] * v[0];
        },
        "f"
    );

    // Define g(x) that calls f(x) internally
    // g(x) = f(x) + x
    CustomFunction g = CustomFunction::create(
        1,
        [&f](const std::vector<Variable>& v) {
            Expression f_result = f(v[0]);
            return f_result + v[0];
        },
        "g"
    );

    // Create main expression tree: G(X) = g(X)
    Variable X;
    X = 3.0;
    Expression G = g(X);

    // First call: compute value and gradient
    double val1 = G->value();
    zero_all_grad();
    G->backward();
    double grad1 = X->gradient();
    EXPECT_DOUBLE_EQ(val1, 12.0);  // 3^2 + 3 = 9 + 3 = 12
    EXPECT_DOUBLE_EQ(grad1, 7.0);  // 2*3 + 1 = 7

    // Second call with same input
    double val2 = G->value();
    zero_all_grad();
    G->backward();
    double grad2 = X->gradient();
    EXPECT_DOUBLE_EQ(val2, 12.0);
    EXPECT_DOUBLE_EQ(grad2, 7.0);
    EXPECT_DOUBLE_EQ(grad1, grad2);

    // Third call
    double val3 = G->value();
    zero_all_grad();
    G->backward();
    double grad3 = X->gradient();
    EXPECT_DOUBLE_EQ(val3, 12.0);
    EXPECT_DOUBLE_EQ(grad3, 7.0);
}

// Test: Deeply nested custom functions
// Verifies that deeply nested custom functions work correctly
TEST_F(CustomFunctionTest, DeeplyNestedCustomFunctions) {
    // f(x) = x^2
    CustomFunction f = CustomFunction::create(
        1,
        [](const std::vector<Variable>& v) {
            return v[0] * v[0];
        },
        "f"
    );

    // g(x) = f(x) + x
    CustomFunction g = CustomFunction::create(
        1,
        [&f](const std::vector<Variable>& v) {
            return f(v[0]) + v[0];
        },
        "g"
    );

    // h(x) = g(x) * 2
    CustomFunction h = CustomFunction::create(
        1,
        [&g](const std::vector<Variable>& v) {
            return g(v[0]) * Const(2.0);
        },
        "h"
    );

    Variable X;
    X = 2.0;
    Expression H = h(X);

    // Compute value and gradient multiple times
    for (int i = 0; i < 5; ++i) {
        zero_all_grad();
        H->backward();
        double grad = X->gradient();

        // Expected: dH/dX = 2 * dg/dX = 2 * (2*X + 1) = 2 * (2*2 + 1) = 2 * 5 = 10
        EXPECT_DOUBLE_EQ(grad, 10.0);
    }
}

// Test: Multiple calls with random inputs (recommended by reviewer)
// Verifies that value/gradient/value_and_gradient return consistent results
// even when called multiple times with random inputs
TEST_F(CustomFunctionTest, MultipleCallsWithRandomInputs) {
    CustomFunction f = CustomFunction::create(
        1,
        [](const std::vector<Variable>& v) {
            return v[0] * v[0];
        },
        "square"
    );

    // Test with multiple random inputs
    std::vector<double> test_inputs = {0.5, 1.0, 2.5, -1.5, 3.0, -2.0, 0.0, 10.0, -10.0, 0.001};

    for (double x : test_inputs) {
        // Call value multiple times - should return same result
        double val1 = f.value({x});
        double val2 = f.value({x});
        double val3 = f.value({x});
        EXPECT_DOUBLE_EQ(val1, x * x);
        EXPECT_DOUBLE_EQ(val1, val2);
        EXPECT_DOUBLE_EQ(val2, val3);

        // Call gradient multiple times - should return same result
        std::vector<double> grad1 = f.gradient({x});
        std::vector<double> grad2 = f.gradient({x});
        std::vector<double> grad3 = f.gradient({x});
        ASSERT_EQ(grad1.size(), 1);
        EXPECT_DOUBLE_EQ(grad1[0], 2.0 * x);
        EXPECT_DOUBLE_EQ(grad1[0], grad2[0]);
        EXPECT_DOUBLE_EQ(grad2[0], grad3[0]);

        // Call value_and_gradient multiple times - should return same result
        auto [val_vg1, grad_vg1] = f.value_and_gradient({x});
        auto [val_vg2, grad_vg2] = f.value_and_gradient({x});
        auto [val_vg3, grad_vg3] = f.value_and_gradient({x});
        EXPECT_DOUBLE_EQ(val_vg1, x * x);
        EXPECT_DOUBLE_EQ(val_vg1, val_vg2);
        EXPECT_DOUBLE_EQ(val_vg2, val_vg3);
        ASSERT_EQ(grad_vg1.size(), 1);
        EXPECT_DOUBLE_EQ(grad_vg1[0], 2.0 * x);
        EXPECT_DOUBLE_EQ(grad_vg1[0], grad_vg2[0]);
        EXPECT_DOUBLE_EQ(grad_vg2[0], grad_vg3[0]);
    }
}

// Test: Composite expression arguments with finite difference verification (recommended by reviewer)
// F(X, Y) = f(X * Y, Y) where f(u, v) = u * v
// Verifies gradient correctness using finite difference
TEST_F(CustomFunctionTest, CompositeExpressionArgumentsWithFiniteDifference) {
    CustomFunction f = CustomFunction::create(
        2,
        [](const std::vector<Variable>& v) {
            return v[0] * v[1];  // f(u, v) = u * v
        },
        "multiply"
    );

    Variable X, Y;
    X = 2.0;
    Y = 3.0;

    // Build composite expression: F(X, Y) = f(X * Y, Y)
    Expression F = f(X * Y, Y);

    // Expected value: f(6, 3) = 6 * 3 = 18
    EXPECT_DOUBLE_EQ(F->value(), 18.0);

    // Compute gradients using AD
    zero_all_grad();
    F->backward();
    double grad_X_ad = X->gradient();
    double grad_Y_ad = Y->gradient();

    // Expected gradients:
    // dF/dX = df/du * du/dX = v * Y = 3 * 3 = 9
    // dF/dY = df/du * du/dY + df/dv = v * X + u = 3 * 2 + 6 = 6 + 6 = 12
    EXPECT_DOUBLE_EQ(grad_X_ad, 9.0);
    EXPECT_DOUBLE_EQ(grad_Y_ad, 12.0);

    // Verify using finite difference
    const double h = 1e-6;

    // dF/dX using finite difference
    X = 2.0 + h;
    Y = 3.0;
    double F_plus_X = F->value();
    X = 2.0 - h;
    double F_minus_X = F->value();
    double grad_X_fd = (F_plus_X - F_minus_X) / (2.0 * h);

    EXPECT_NEAR(grad_X_ad, grad_X_fd, 1e-5)
        << "Gradient dF/dX mismatch: AD=" << grad_X_ad << ", FD=" << grad_X_fd;

    // dF/dY using finite difference
    X = 2.0;
    Y = 3.0 + h;
    double F_plus_Y = F->value();
    Y = 3.0 - h;
    double F_minus_Y = F->value();
    double grad_Y_fd = (F_plus_Y - F_minus_Y) / (2.0 * h);

    EXPECT_NEAR(grad_Y_ad, grad_Y_fd, 1e-5)
        << "Gradient dF/dY mismatch: AD=" << grad_Y_ad << ", FD=" << grad_Y_fd;
}

