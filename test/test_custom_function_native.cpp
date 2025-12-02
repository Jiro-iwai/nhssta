// -*- c++ -*-
// Test cases for Native Custom Function feature
// Author: Jiro Iwai
//
// Tests for Native type custom functions (C++ functions/lambdas)
// Based on custom_native_function_spec.md section 9.2-9.4

#include <gtest/gtest.h>
#include <cmath>
#include <vector>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#include "../src/expression.hpp"
#include "statistical_functions.hpp"
using namespace RandomVariable;

class CustomFunctionNativeTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Native type tests don't use Expression tree, so zero_all_grad() is not needed
        // But we can call it for consistency if needed
    }

    void TearDown() override {
        // Clean up if needed
    }
};

// Test 5: 1変数 f(x) = sin(x), f'(x) = cos(x)（Native）
// From spec section 9.2
TEST_F(CustomFunctionNativeTest, SingleVariableSinNative) {
    // Create Native type custom function with separate value and gradient
    CustomFunction f = CustomFunction::create_native(
        1,
        [](const std::vector<double>& x) -> double {
            return std::sin(x[0]);
        },
        [](const std::vector<double>& x) -> std::vector<double> {
            return {std::cos(x[0])};
        },
        "sin_native"
    );

    EXPECT_EQ(f.input_dim(), 1);
    EXPECT_EQ(f.name(), "sin_native");
    EXPECT_TRUE(f.valid());

    // Test with random x values
    std::vector<double> test_values = {0.0, M_PI / 2.0, M_PI, -M_PI / 4.0, 1.5};
    
    for (double x_val : test_values) {
        std::vector<double> x = {x_val};
        
        // Test value
        double expected_val = std::sin(x_val);
        EXPECT_NEAR(f.value(x), expected_val, 1e-10) << "Value mismatch for x=" << x_val;
        
        // Test gradient
        std::vector<double> grad = f.gradient(x);
        ASSERT_EQ(grad.size(), 1);
        double expected_grad = std::cos(x_val);
        EXPECT_NEAR(grad[0], expected_grad, 1e-10) << "Gradient mismatch for x=" << x_val;
        
        // Test value_and_gradient
        auto [val, grad_vec] = f.value_and_gradient(x);
        EXPECT_NEAR(val, expected_val, 1e-10);
        ASSERT_EQ(grad_vec.size(), 1);
        EXPECT_NEAR(grad_vec[0], expected_grad, 1e-10);
    }
}

// Test 6: 2変数 f(x,y) = x^2 + y^3（Native, value_and_gradient まとめ版）
// From spec section 9.2
TEST_F(CustomFunctionNativeTest, TwoVariableValueAndGradientCombined) {
    // Create Native type with value_and_gradient combined
    CustomFunction f = CustomFunction::create_native(
        2,
        [](const std::vector<double>& x) -> std::pair<double, std::vector<double>> {
            double val = x[0] * x[0] + x[1] * x[1] * x[1];
            std::vector<double> grad = {2.0 * x[0], 3.0 * x[1] * x[1]};
            return {val, grad};
        },
        "f_xy_native"
    );

    EXPECT_EQ(f.input_dim(), 2);
    EXPECT_TRUE(f.valid());

    // Test with various inputs
    std::vector<std::vector<double>> test_inputs = {
        {0.0, 0.0},
        {1.0, 1.0},
        {2.0, 3.0},
        {-1.0, 2.0}
    };

    for (const auto& x : test_inputs) {
        double expected_val = x[0] * x[0] + x[1] * x[1] * x[1];
        std::vector<double> expected_grad = {2.0 * x[0], 3.0 * x[1] * x[1]};
        
        // Test value
        EXPECT_NEAR(f.value(x), expected_val, 1e-10);
        
        // Test gradient
        std::vector<double> grad = f.gradient(x);
        ASSERT_EQ(grad.size(), 2);
        EXPECT_NEAR(grad[0], expected_grad[0], 1e-10);
        EXPECT_NEAR(grad[1], expected_grad[1], 1e-10);
        
        // Test value_and_gradient
        auto [val, grad_vec] = f.value_and_gradient(x);
        EXPECT_NEAR(val, expected_val, 1e-10);
        ASSERT_EQ(grad_vec.size(), 2);
        EXPECT_NEAR(grad_vec[0], expected_grad[0], 1e-10);
        EXPECT_NEAR(grad_vec[1], expected_grad[1], 1e-10);
    }
}

// Test 7: メイン式木から Native 関数を呼ぶ
// From spec section 9.3
TEST_F(CustomFunctionNativeTest, NativeFunctionInMainExpressionTree) {
    // f(x,y) = x*y + x² を Native 型で定義
    CustomFunction f = CustomFunction::create_native(
        2,
        [](const std::vector<double>& x) -> double {
            return x[0] * x[1] + x[0] * x[0];
        },
        [](const std::vector<double>& x) -> std::vector<double> {
            return {x[1] + 2.0 * x[0], x[0]};
        },
        "f_xy_native"
    );

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

// Test 8: 複合式を Native に渡す
// From spec section 9.3
TEST_F(CustomFunctionNativeTest, CompositeExpressionArguments) {
    // f(u, v) = u² + v （Native）
    CustomFunction f = CustomFunction::create_native(
        2,
        [](const std::vector<double>& x) -> double {
            return x[0] * x[0] + x[1];
        },
        [](const std::vector<double>& x) -> std::vector<double> {
            return {2.0 * x[0], 1.0};
        },
        "f_uv_native"
    );

    // Create main expression tree variables
    Variable X, Y;
    X = 2.0;
    Y = 3.0;

    // Build main expression: F = f(X * Y, Y)
    Expression F = f(X * Y, Y);

    // Test value: f(2*3, 3) = f(6, 3) = 6² + 3 = 36 + 3 = 39
    EXPECT_DOUBLE_EQ(F->value(), 39.0);

    // Compute gradients
    zero_all_grad();
    F->backward();

    // Expected gradients:
    // F = f(X * Y, Y) where f(u, v) = u² + v
    // F = (X * Y)² + Y = X²Y² + Y
    // dF/dX = 2XY² = 2 * 2 * 9 = 36
    // dF/dY = 2X²Y + 1 = 2 * 4 * 3 + 1 = 25
    
    // Verify gradients using finite differences
    const double h = 1e-5;
    
    // dF/dX via finite difference
    zero_all_grad();
    X = 2.0 + h;
    Y = 3.0;
    double val_plus_x = F->value();
    
    X = 2.0 - h;
    double val_minus_x = F->value();
    double numerical_grad_x = (val_plus_x - val_minus_x) / (2.0 * h);
    
    X = 2.0;
    zero_all_grad();
    F->backward();
    EXPECT_NEAR(X->gradient(), numerical_grad_x, 1e-3) << "Gradient mismatch for X: expected ~" << numerical_grad_x << ", got " << X->gradient();
    
    // dF/dY via finite difference
    zero_all_grad();
    X = 2.0;
    Y = 3.0 + h;
    double val_plus_y = F->value();
    
    Y = 3.0 - h;
    double val_minus_y = F->value();
    double numerical_grad_y = (val_plus_y - val_minus_y) / (2.0 * h);
    
    Y = 3.0;
    zero_all_grad();
    F->backward();
    EXPECT_NEAR(Y->gradient(), numerical_grad_y, 1e-3) << "Gradient mismatch for Y: expected ~" << numerical_grad_y << ", got " << Y->gradient();
}

// Test 9: Graph / Native 両方で同じ関数を定義し、結果が一致するか
// From spec section 9.3
TEST_F(CustomFunctionNativeTest, GraphVsNativeConsistency) {
    // f(x,y) = x*y + x² を Graph と Native の両方で定義
    CustomFunction f_graph = CustomFunction::create(
        2,
        [](const std::vector<Variable>& v) {
            const auto& x = v[0];
            const auto& y = v[1];
            return x * y + x * x;
        },
        "f_xy_graph"
    );

    CustomFunction f_native = CustomFunction::create_native(
        2,
        [](const std::vector<double>& x) -> double {
            return x[0] * x[1] + x[0] * x[0];
        },
        [](const std::vector<double>& x) -> std::vector<double> {
            return {x[1] + 2.0 * x[0], x[0]};
        },
        "f_xy_native"
    );

    // Test with various inputs
    std::vector<std::vector<double>> test_inputs = {
        {0.0, 0.0},
        {1.0, 1.0},
        {2.0, 3.0},
        {-1.0, 2.0},
        {5.0, -3.0}
    };

    const double tolerance = 1e-10;

    for (const auto& x : test_inputs) {
        // Compare values
        double val_graph = f_graph.value(x);
        double val_native = f_native.value(x);
        EXPECT_NEAR(val_graph, val_native, tolerance)
            << "Value mismatch for input (" << x[0] << ", " << x[1] << ")";

        // Compare gradients
        std::vector<double> grad_graph = f_graph.gradient(x);
        std::vector<double> grad_native = f_native.gradient(x);
        ASSERT_EQ(grad_graph.size(), grad_native.size());
        for (size_t i = 0; i < grad_graph.size(); ++i) {
            EXPECT_NEAR(grad_graph[i], grad_native[i], tolerance)
                << "Gradient mismatch for variable " << i
                << " with input (" << x[0] << ", " << x[1] << ")";
        }
    }
}

// Test 10: F.value(); F.backward(); のシーケンス（キャッシュ挙動）
// From spec section 9.4
TEST_F(CustomFunctionNativeTest, ValueThenBackwardSequence) {
    CustomFunction f = CustomFunction::create_native(
        1,
        [](const std::vector<double>& x) -> double {
            return x[0] * x[0];
        },
        [](const std::vector<double>& x) -> std::vector<double> {
            return {2.0 * x[0]};
        },
        "square_native"
    );

    Variable X;
    X = 3.0;

    Expression F = f(X);

    // Call value() then backward() multiple times
    for (int i = 0; i < 5; ++i) {
        zero_all_grad();
        double val1 = F->value();
        F->backward();
        double grad1 = X->gradient();

        // Call again - should get same results
        zero_all_grad();
        double val2 = F->value();
        F->backward();
        double grad2 = X->gradient();

        EXPECT_DOUBLE_EQ(val1, val2) << "Value should be consistent (iteration " << i << ")";
        EXPECT_DOUBLE_EQ(grad1, grad2) << "Gradient should be consistent (iteration " << i << ")";
        EXPECT_DOUBLE_EQ(val1, 9.0);
        EXPECT_DOUBLE_EQ(grad1, 6.0);
    }
}

// Test 11: 同じ CustomFunction を複数ノードから異なる引数で呼ぶ
// From spec section 9.4
TEST_F(CustomFunctionNativeTest, MultipleCallsWithDifferentArguments) {
    CustomFunction f = CustomFunction::create_native(
        2,
        [](const std::vector<double>& x) -> double {
            return x[0] * x[1];
        },
        [](const std::vector<double>& x) -> std::vector<double> {
            return {x[1], x[0]};
        },
        "multiply_native"
    );

    Variable X, Y, Z;
    X = 1.0;
    Y = 2.0;
    Z = 3.0;

    // F = f(X,Y) + f(Y,Z) + f(X,Z)
    Expression F = f(X, Y) + f(Y, Z) + f(X, Z);

    // f(X,Y) = 1*2 = 2
    // f(Y,Z) = 2*3 = 6
    // f(X,Z) = 1*3 = 3
    // F = 2 + 6 + 3 = 11
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

// Test: Error handling - null function pointers
TEST_F(CustomFunctionNativeTest, NullFunctionPointers) {
    // This test will fail until implementation is complete
    // It documents the expected error behavior
    
    // Should throw when value function is null
    EXPECT_THROW({
        (void)CustomFunction::create_native(
            1,
            nullptr,
            [](const std::vector<double>&) -> std::vector<double> { return {}; },
            "test"
        );
    }, Nh::RuntimeException);
    
    // Should throw when gradient function is null
    EXPECT_THROW({
        (void)CustomFunction::create_native(
            1,
            [](const std::vector<double>&) -> double { return 0.0; },
            nullptr,
            "test"
        );
    }, Nh::RuntimeException);
}

// Test: Error handling - input dimension mismatch
TEST_F(CustomFunctionNativeTest, InputDimensionMismatch) {
    CustomFunction f = CustomFunction::create_native(
        2,
        [](const std::vector<double>& x) -> double {
            return x[0] + x[1];
        },
        [](const std::vector<double>& x) -> std::vector<double> {
            return {1.0, 1.0};
        },
        "add_native"
    );

    // Should throw when input size doesn't match input_dim
    EXPECT_THROW({
        (void)f.value({1.0});  // Wrong size: 1 instead of 2
    }, Nh::RuntimeException);

    EXPECT_THROW({
        (void)f.gradient({1.0, 2.0, 3.0});  // Wrong size: 3 instead of 2
    }, Nh::RuntimeException);
}

