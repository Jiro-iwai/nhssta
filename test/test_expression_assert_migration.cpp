// -*- c++ -*-
// Unit tests for Expression class migrated from src/test.C
// Issue #10: 既存assertテストの移行
// Note: Automatic differentiation (d() method) has been removed

#include <gtest/gtest.h>

#include <cmath>

#include "../src/expression.hpp"

using namespace Nh;

class ExpressionAssertMigrationTest : public ::testing::Test {
   protected:
    void SetUp() override {
        // Clear expression table before each test
        // Note: Expression uses static table, so we need to be careful
    }

    void TearDown() override {
        // Cleanup if needed
    }
};

// Test: Multiplication by constant (2.0*x)
TEST_F(ExpressionAssertMigrationTest, TestMultiplicationByConstant) {
    Variable x;
    Expression z;
    double v;

    z = 2.0 * x;

    x = 1.0;
    EXPECT_DOUBLE_EQ((v << z), 2.0);

    x = 2.0;
    EXPECT_DOUBLE_EQ((v << z), 4.0);
}

// Test: Addition (x + y)
TEST_F(ExpressionAssertMigrationTest, TestAddition) {
    Variable x, y;
    Expression z;
    double v;

    z = x + y;

    x = 1.0;
    y = 1.0;
    EXPECT_DOUBLE_EQ((v << z), 2.0);

    x = 2.0;
    y = -1.0;
    EXPECT_DOUBLE_EQ((v << z), 1.0);
}

// Test: Multiplication (x*y)
TEST_F(ExpressionAssertMigrationTest, TestMultiplication) {
    Variable x, y;
    Expression z;
    double v;

    z = x * y;

    x = 3.0;
    y = 4.0;
    EXPECT_DOUBLE_EQ((v << z), 12.0);

    x = -1.0;
    y = 2.0;
    EXPECT_DOUBLE_EQ((v << z), -2.0);
}

// Test: Subtraction (x - y)
TEST_F(ExpressionAssertMigrationTest, TestSubtraction) {
    Variable x, y;
    Expression z;
    double v;

    z = x - y;
    x = 2.0;
    y = 1.0;

    EXPECT_DOUBLE_EQ((v << z), 1.0);
}

// Test: Division (x/y)
TEST_F(ExpressionAssertMigrationTest, TestDivision) {
    Variable x, y;
    Expression z;
    double v;

    z = x / y;
    x = 2.0;
    y = 1.0;

    EXPECT_DOUBLE_EQ((v << z), 2.0);
}

// Test: Triple addition (x + y + w)
TEST_F(ExpressionAssertMigrationTest, TestTripleAddition) {
    Variable x, y, w;
    Expression z;
    double v;

    z = x + y + w;
    x = 2.0;
    y = 1.0;
    w = 1.0;

    EXPECT_DOUBLE_EQ((v << z), 4.0);
}

// Test: Triple multiplication (x*y*w)
TEST_F(ExpressionAssertMigrationTest, TestTripleMultiplication) {
    Variable x, y, w;
    Expression z;
    double v;

    z = x * y * w;
    x = 2.0;
    y = 1.0;
    w = 1.0;

    EXPECT_DOUBLE_EQ((v << z), 2.0);
}

// Test: Exponential function (exp(x))
TEST_F(ExpressionAssertMigrationTest, TestExponential) {
    Variable x;
    Expression z;
    double v;

    z = exp(x);
    x = 0.0;
    EXPECT_DOUBLE_EQ((v << x), 0.0);
    EXPECT_DOUBLE_EQ((v << z), 1.0);
}

// Test: Power function (x^2)
TEST_F(ExpressionAssertMigrationTest, TestPower) {
    Variable x;
    Expression z;
    double v;

    z = x ^ 2;
    x = 2.0;

    EXPECT_DOUBLE_EQ((v << x), 2.0);
    EXPECT_DOUBLE_EQ((v << z), 4.0);
}

// Test: Complex expression (log(x*x*y/w*w)*exp(x)/w)
// Note: Test that expression evaluation doesn't crash
TEST_F(ExpressionAssertMigrationTest, TestComplexExpression) {
    Variable x, y, w;
    Expression z;
    double v;

    z = log(x * x * y / w * w) * exp(x) / w;

    // Set variable values before evaluating
    x = 1.0;
    y = 1.0;
    w = 1.0;

    // Verify that we can evaluate the expression
    EXPECT_DOUBLE_EQ((v << x), 1.0);
    // log(1*1*1/1*1) * exp(1) / 1 = log(1) * e / 1 = 0 * e = 0
    EXPECT_DOUBLE_EQ((v << z), 0.0);
}

TEST_F(ExpressionAssertMigrationTest, ExpressionSurvivesOperandScope) {
    Expression expr;
    double value = 0.0;

    {
        Variable x;
        x = 4.0;
        expr = x + 1.0;
    }

    EXPECT_DOUBLE_EQ((value << expr), 5.0);
}

TEST_F(ExpressionAssertMigrationTest, SharedSubexpressionEvaluatesConsistently) {
    Variable x;
    Expression sub;
    Expression expr1;
    Expression expr2;
    double value = 0.0;

    x = 2.0;
    sub = x + 1.0;
    expr1 = sub + 5.0;
    expr2 = sub * 3.0;

    EXPECT_DOUBLE_EQ((value << expr1), 8.0);
    EXPECT_DOUBLE_EQ((value << expr2), 9.0);

    x = 3.0;
    EXPECT_DOUBLE_EQ((value << expr1), 9.0);
    EXPECT_DOUBLE_EQ((value << expr2), 12.0);
}
