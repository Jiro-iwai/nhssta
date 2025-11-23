#include <gtest/gtest.h>
#include "../src/util_numerical.hpp"
#include <cmath>

using namespace RandomVariable;

class UtilTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Setup code if needed
    }

    void TearDown() override {
        // Cleanup code if needed
    }
};

// Test MeanMax with negative value (should return 0.0)
TEST_F(UtilTest, MeanMaxNegativeValue) {
    double result = MeanMax(-10.0);
    EXPECT_DOUBLE_EQ(result, 0.0);
}

// Test MeanMax with large positive value (should return the value itself)
TEST_F(UtilTest, MeanMaxLargePositiveValue) {
    double result = MeanMax(10.0);
    EXPECT_DOUBLE_EQ(result, 10.0);
}

// Test MeanMax with zero
TEST_F(UtilTest, MeanMaxZero) {
    double result = MeanMax(0.0);
    EXPECT_GE(result, 0.0);
    EXPECT_LE(result, 1.0);
}

// Test MeanMax with values in range [-5, 5]
TEST_F(UtilTest, MeanMaxInRange) {
    // Test a few values in the interpolation range
    double result1 = MeanMax(-2.5);
    EXPECT_GE(result1, 0.0);
    EXPECT_LE(result1, 1.0);
    
    double result2 = MeanMax(2.5);
    EXPECT_GE(result2, 0.0);
    EXPECT_LE(result2, 5.0);
    
    double result3 = MeanMax(1.0);
    EXPECT_GE(result3, 0.0);
    EXPECT_LE(result3, 5.0);
}

// Test MeanPhiMax with negative value (should return 1.0)
TEST_F(UtilTest, MeanPhiMaxNegativeValue) {
    double result = MeanPhiMax(-10.0);
    EXPECT_DOUBLE_EQ(result, 1.0);
}

// Test MeanPhiMax with large positive value (should return 0.0)
TEST_F(UtilTest, MeanPhiMaxLargePositiveValue) {
    double result = MeanPhiMax(10.0);
    EXPECT_DOUBLE_EQ(result, 0.0);
}

// Test MeanPhiMax with zero
TEST_F(UtilTest, MeanPhiMaxZero) {
    double result = MeanPhiMax(0.0);
    EXPECT_GE(result, 0.0);
    EXPECT_LE(result, 1.0);
}

// Test MeanPhiMax with values in range [-5, 5]
TEST_F(UtilTest, MeanPhiMaxInRange) {
    // Test a few values in the interpolation range
    double result1 = MeanPhiMax(-2.5);
    EXPECT_GE(result1, 0.0);
    EXPECT_LE(result1, 1.0);
    
    double result2 = MeanPhiMax(2.5);
    EXPECT_GE(result2, 0.0);
    EXPECT_LE(result2, 1.0);
    
    double result3 = MeanPhiMax(1.0);
    EXPECT_GE(result3, 0.0);
    EXPECT_LE(result3, 1.0);
}

// Test MeanMax2 with negative value (should return 1.0)
TEST_F(UtilTest, MeanMax2NegativeValue) {
    double result = MeanMax2(-10.0);
    EXPECT_DOUBLE_EQ(result, 1.0);
}

// Test MeanMax2 with large positive value (should return a^2)
TEST_F(UtilTest, MeanMax2LargePositiveValue) {
    double a = 10.0;
    double result = MeanMax2(a);
    EXPECT_DOUBLE_EQ(result, a * a);
}

// Test MeanMax2 with zero
TEST_F(UtilTest, MeanMax2Zero) {
    double result = MeanMax2(0.0);
    EXPECT_GE(result, 0.0);
    EXPECT_LE(result, 1.0);
}

// Test MeanMax2 with values in range [-5, 5]
TEST_F(UtilTest, MeanMax2InRange) {
    // Test a few values in the interpolation range
    double result1 = MeanMax2(-2.5);
    EXPECT_GE(result1, 0.0);
    EXPECT_LE(result1, 10.0);
    
    double result2 = MeanMax2(2.5);
    EXPECT_GE(result2, 0.0);
    EXPECT_LE(result2, 10.0);
    
    double result3 = MeanMax2(1.0);
    EXPECT_GE(result3, 0.0);
    EXPECT_LE(result3, 10.0);
}

// Test monotonicity: MeanMax should be monotonic increasing
TEST_F(UtilTest, MeanMaxMonotonicity) {
    double a1 = -2.0;
    double a2 = -1.0;
    double a3 = 0.0;
    double a4 = 1.0;
    double a5 = 2.0;
    
    double r1 = MeanMax(a1);
    double r2 = MeanMax(a2);
    double r3 = MeanMax(a3);
    double r4 = MeanMax(a4);
    double r5 = MeanMax(a5);
    
    EXPECT_LE(r1, r2);
    EXPECT_LE(r2, r3);
    EXPECT_LE(r3, r4);
    EXPECT_LE(r4, r5);
}

// Test boundary values
TEST_F(UtilTest, MeanMaxBoundaryValues) {
    double result_minus5 = MeanMax(-5.0);
    EXPECT_GE(result_minus5, 0.0);
    
    double result_plus5 = MeanMax(5.0);
    EXPECT_LE(result_plus5, 5.0);
}

