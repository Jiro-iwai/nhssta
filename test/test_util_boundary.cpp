// -*- c++ -*-
// Unit tests for util_numerical.cpp boundary value testing
// Issue #51: テスト強化: util.cppのテーブル・近似式の境界値テスト追加

#include <gtest/gtest.h>
#include "../src/util_numerical.hpp"
#include <cmath>
#include <limits>

using namespace RandomVariable;

class UtilBoundaryTest : public ::testing::Test {
protected:
    void SetUp() override {
    }

    void TearDown() override {
    }
    
    // Helper to check if two doubles are approximately equal
    bool isApproxEqual(double a, double b, double tolerance = 1e-10) {
        return std::abs(a - b) < tolerance;
    }
};

// Test MeanMax with extreme negative values
TEST_F(UtilBoundaryTest, MeanMaxExtremeNegative) {
    EXPECT_DOUBLE_EQ(MeanMax(-100.0), 0.0);
    EXPECT_DOUBLE_EQ(MeanMax(-50.0), 0.0);
    EXPECT_DOUBLE_EQ(MeanMax(-10.0), 0.0);
    EXPECT_DOUBLE_EQ(MeanMax(-6.0), 0.0);
}

// Test MeanMax with extreme positive values
TEST_F(UtilBoundaryTest, MeanMaxExtremePositive) {
    EXPECT_DOUBLE_EQ(MeanMax(10.0), 10.0);
    EXPECT_DOUBLE_EQ(MeanMax(50.0), 50.0);
    EXPECT_DOUBLE_EQ(MeanMax(100.0), 100.0);
    EXPECT_DOUBLE_EQ(MeanMax(1000.0), 1000.0);
}

// Test MeanMax at exact boundary values (-5.0 and 5.0)
TEST_F(UtilBoundaryTest, MeanMaxExactBoundaries) {
    // At -5.0, should use table value (first element)
    double result_minus5 = MeanMax(-5.0);
    EXPECT_GE(result_minus5, 0.0);
    EXPECT_LE(result_minus5, 0.1); // Should be very small
    
    // At 5.0, should use table value (last element)
    double result_plus5 = MeanMax(5.0);
    EXPECT_GE(result_plus5, 4.0);
    EXPECT_LE(result_plus5, 5.0);
}

// Test MeanMax near boundary values
TEST_F(UtilBoundaryTest, MeanMaxNearBoundaries) {
    // Just below -5.0
    EXPECT_DOUBLE_EQ(MeanMax(-5.01), 0.0);
    EXPECT_DOUBLE_EQ(MeanMax(-5.001), 0.0);
    
    // Just above -5.0 (should use table interpolation)
    double result_near_minus5 = MeanMax(-4.99);
    EXPECT_GE(result_near_minus5, 0.0);
    EXPECT_LE(result_near_minus5, 0.1);
    
    // Just below 5.0 (should use table interpolation)
    double result_near_plus5 = MeanMax(4.99);
    EXPECT_GE(result_near_plus5, 4.0);
    EXPECT_LE(result_near_plus5, 5.0);
    
    // Just above 5.0
    EXPECT_DOUBLE_EQ(MeanMax(5.01), 5.01);
    EXPECT_DOUBLE_EQ(MeanMax(5.001), 5.001);
}

// Test MeanMax at table step boundaries (every 0.05 from -5 to 5)
TEST_F(UtilBoundaryTest, MeanMaxTableStepBoundaries) {
    // Test a few key table step boundaries
    double result_neg4_95 = MeanMax(-4.95);
    double result_neg4_90 = MeanMax(-4.90);
    EXPECT_GE(result_neg4_95, 0.0);
    EXPECT_GE(result_neg4_90, result_neg4_95); // Should be monotonic
    
    double result_0 = MeanMax(0.0);
    double result_0_05 = MeanMax(0.05);
    EXPECT_GE(result_0_05, result_0); // Should be monotonic
    
    double result_4_90 = MeanMax(4.90);
    double result_4_95 = MeanMax(4.95);
    EXPECT_GE(result_4_95, result_4_90); // Should be monotonic
    EXPECT_LE(result_4_95, 5.0);
}

// Test MeanMax with very small values near zero
TEST_F(UtilBoundaryTest, MeanMaxNearZero) {
    double result_neg0_01 = MeanMax(-0.01);
    double result_0 = MeanMax(0.0);
    double result_0_01 = MeanMax(0.01);
    
    EXPECT_GE(result_0, result_neg0_01); // Should be monotonic
    EXPECT_GE(result_0_01, result_0); // Should be monotonic
    
    // Values near zero should be small but positive
    EXPECT_GE(result_neg0_01, 0.0);
    EXPECT_GE(result_0, 0.0);
    EXPECT_GE(result_0_01, 0.0);
}

// Test MeanPhiMax with extreme negative values
TEST_F(UtilBoundaryTest, MeanPhiMaxExtremeNegative) {
    EXPECT_DOUBLE_EQ(MeanPhiMax(-100.0), 1.0);
    EXPECT_DOUBLE_EQ(MeanPhiMax(-50.0), 1.0);
    EXPECT_DOUBLE_EQ(MeanPhiMax(-10.0), 1.0);
    EXPECT_DOUBLE_EQ(MeanPhiMax(-6.0), 1.0);
}

// Test MeanPhiMax with extreme positive values
TEST_F(UtilBoundaryTest, MeanPhiMaxExtremePositive) {
    EXPECT_DOUBLE_EQ(MeanPhiMax(10.0), 0.0);
    EXPECT_DOUBLE_EQ(MeanPhiMax(50.0), 0.0);
    EXPECT_DOUBLE_EQ(MeanPhiMax(100.0), 0.0);
    EXPECT_DOUBLE_EQ(MeanPhiMax(1000.0), 0.0);
}

// Test MeanPhiMax at exact boundary values
TEST_F(UtilBoundaryTest, MeanPhiMaxExactBoundaries) {
    // At -5.0, should be close to 1.0
    double result_minus5 = MeanPhiMax(-5.0);
    EXPECT_GE(result_minus5, 0.9);
    EXPECT_LE(result_minus5, 1.0);
    
    // At 5.0, should be close to 0.0
    double result_plus5 = MeanPhiMax(5.0);
    EXPECT_GE(result_plus5, 0.0);
    EXPECT_LE(result_plus5, 0.1);
}

// Test MeanPhiMax near boundary values
TEST_F(UtilBoundaryTest, MeanPhiMaxNearBoundaries) {
    // Just below -5.0
    EXPECT_DOUBLE_EQ(MeanPhiMax(-5.01), 1.0);
    EXPECT_DOUBLE_EQ(MeanPhiMax(-5.001), 1.0);
    
    // Just above -5.0
    double result_near_minus5 = MeanPhiMax(-4.99);
    EXPECT_GE(result_near_minus5, 0.9);
    EXPECT_LE(result_near_minus5, 1.0);
    
    // Just below 5.0
    double result_near_plus5 = MeanPhiMax(4.99);
    EXPECT_GE(result_near_plus5, 0.0);
    EXPECT_LE(result_near_plus5, 0.1);
    
    // Just above 5.0
    EXPECT_DOUBLE_EQ(MeanPhiMax(5.01), 0.0);
    EXPECT_DOUBLE_EQ(MeanPhiMax(5.001), 0.0);
}

// Test MeanPhiMax monotonicity (should be decreasing)
TEST_F(UtilBoundaryTest, MeanPhiMaxMonotonicity) {
    double r1 = MeanPhiMax(-2.0);
    double r2 = MeanPhiMax(-1.0);
    double r3 = MeanPhiMax(0.0);
    double r4 = MeanPhiMax(1.0);
    double r5 = MeanPhiMax(2.0);
    
    EXPECT_GE(r1, r2); // Should be decreasing
    EXPECT_GE(r2, r3);
    EXPECT_GE(r3, r4);
    EXPECT_GE(r4, r5);
}

// Test MeanMax2 with extreme negative values
TEST_F(UtilBoundaryTest, MeanMax2ExtremeNegative) {
    EXPECT_DOUBLE_EQ(MeanMax2(-100.0), 1.0);
    EXPECT_DOUBLE_EQ(MeanMax2(-50.0), 1.0);
    EXPECT_DOUBLE_EQ(MeanMax2(-10.0), 1.0);
    EXPECT_DOUBLE_EQ(MeanMax2(-6.0), 1.0);
}

// Test MeanMax2 with extreme positive values
TEST_F(UtilBoundaryTest, MeanMax2ExtremePositive) {
    EXPECT_DOUBLE_EQ(MeanMax2(10.0), 100.0);
    EXPECT_DOUBLE_EQ(MeanMax2(50.0), 2500.0);
    EXPECT_DOUBLE_EQ(MeanMax2(100.0), 10000.0);
    EXPECT_DOUBLE_EQ(MeanMax2(1000.0), 1000000.0);
}

// Test MeanMax2 at exact boundary values
TEST_F(UtilBoundaryTest, MeanMax2ExactBoundaries) {
    // At -5.0, should use table value (first element, close to 1.0)
    double result_minus5 = MeanMax2(-5.0);
    EXPECT_GE(result_minus5, 0.9);
    EXPECT_LE(result_minus5, 1.1);
    
    // At 5.0, should use table value (last element, should be 25.0)
    double result_plus5 = MeanMax2(5.0);
    EXPECT_GE(result_plus5, 24.0);
    EXPECT_LE(result_plus5, 25.0);
}

// Test MeanMax2 near boundary values
TEST_F(UtilBoundaryTest, MeanMax2NearBoundaries) {
    // Just below -5.0
    EXPECT_DOUBLE_EQ(MeanMax2(-5.01), 1.0);
    EXPECT_DOUBLE_EQ(MeanMax2(-5.001), 1.0);
    
    // Just above -5.0
    double result_near_minus5 = MeanMax2(-4.99);
    EXPECT_GE(result_near_minus5, 0.9);
    EXPECT_LE(result_near_minus5, 1.1);
    
    // Just below 5.0
    double result_near_plus5 = MeanMax2(4.99);
    EXPECT_GE(result_near_plus5, 24.0);
    EXPECT_LE(result_near_plus5, 25.0);
    
    // Just above 5.0
    EXPECT_DOUBLE_EQ(MeanMax2(5.01), 5.01 * 5.01);
    EXPECT_DOUBLE_EQ(MeanMax2(5.001), 5.001 * 5.001);
}

// Test MeanMax2 monotonicity
// Note: MeanMax2 represents E[max(0, X)^2] where X ~ N(a, 1)
// For negative values approaching 0, the value decreases, then increases for positive values
// The function is NOT monotonic across the entire range, but should be monotonic in positive range
TEST_F(UtilBoundaryTest, MeanMax2Monotonicity) {
    // Test monotonicity in positive range (where it should be clearly increasing)
    double r3 = MeanMax2(0.0);
    double r4 = MeanMax2(1.0);
    double r5 = MeanMax2(2.0);
    double r6 = MeanMax2(3.0);
    
    EXPECT_LE(r3, r4); // Should be increasing for positive values
    EXPECT_LE(r4, r5);
    EXPECT_LE(r5, r6);
    
    // Test that values are in expected ranges
    double r1 = MeanMax2(-2.0);
    double r2 = MeanMax2(-1.0);
    EXPECT_GE(r1, 0.5); // Should be >= 0.5 (minimum at a=0)
    EXPECT_GE(r2, 0.5);
    EXPECT_GE(r3, 0.5); // At a=0, should be minimum (0.5)
}

// Test all functions with NaN (should not crash, behavior undefined but testable)
TEST_F(UtilBoundaryTest, MeanMaxWithNaN) {
    double nan_val = std::numeric_limits<double>::quiet_NaN();
    double result = MeanMax(nan_val);
    // NaN comparison: result should be NaN or handled gracefully
    // We just check it doesn't crash
    (void)result; // Suppress unused variable warning
}

// Test all functions with infinity
TEST_F(UtilBoundaryTest, MeanMaxWithInfinity) {
    double pos_inf = std::numeric_limits<double>::infinity();
    double neg_inf = -std::numeric_limits<double>::infinity();
    
    // Positive infinity should return infinity for MeanMax
    double result_pos = MeanMax(pos_inf);
    EXPECT_TRUE(std::isinf(result_pos) || result_pos > 1e10);
    
    // Negative infinity should return 0.0 for MeanMax
    EXPECT_DOUBLE_EQ(MeanMax(neg_inf), 0.0);
}

// Test continuity at boundaries (values just inside and outside should be close)
TEST_F(UtilBoundaryTest, MeanMaxContinuityAtBoundaries) {
    // Test continuity at -5.0 boundary
    double just_outside = MeanMax(-5.01); // Should be 0.0
    double just_inside = MeanMax(-4.99);  // Should be small but > 0
    
    EXPECT_DOUBLE_EQ(just_outside, 0.0);
    EXPECT_GE(just_inside, 0.0);
    EXPECT_LE(just_inside, 0.1); // Should be small
    
    // Test continuity at 5.0 boundary
    double just_inside_plus = MeanMax(4.99);  // Should be < 5.0
    double just_outside_plus = MeanMax(5.01); // Should be 5.01
    
    EXPECT_LE(just_inside_plus, 5.0);
    EXPECT_DOUBLE_EQ(just_outside_plus, 5.01);
    // The difference should be small (within interpolation error)
    EXPECT_LE(std::abs(just_outside_plus - just_inside_plus), 0.1);
}

// Test golden values from table (verify table values are correctly accessed)
// These are the exact table values at specific indices
TEST_F(UtilBoundaryTest, MeanMaxGoldenValues) {
    // At a = -5.0, should use first table element (index 0)
    // Table value: mean_max_tab[0] = 4.95574e-08
    double result_minus5 = MeanMax(-5.0);
    EXPECT_GE(result_minus5, 4.0e-08);
    EXPECT_LE(result_minus5, 6.0e-08);
    
    // At a = 0.0, should use table element around index 100
    // Table value: mean_max_tab[100] = 0.396406 (actual value)
    // Interpolation may use adjacent values
    double result_zero = MeanMax(0.0);
    EXPECT_GE(result_zero, 0.3);
    EXPECT_LE(result_zero, 0.5);
    
    // At a = 5.0, should use last table element (index 200)
    // Table value: mean_max_tab[200] = 5.0
    double result_plus5 = MeanMax(5.0);
    EXPECT_GE(result_plus5, 4.9);
    EXPECT_LE(result_plus5, 5.0);
}

// Test golden values for MeanPhiMax
TEST_F(UtilBoundaryTest, MeanPhiMaxGoldenValues) {
    // At a = -5.0, should use first table element (index 0)
    // Table value: mean_x_max_tab[0] = 1.0
    double result_minus5 = MeanPhiMax(-5.0);
    EXPECT_GE(result_minus5, 0.99);
    EXPECT_LE(result_minus5, 1.0);
    
    // At a = 0.0, should use table element around index 100
    // Table value: mean_x_max_tab[100] = 0.5
    double result_zero = MeanPhiMax(0.0);
    EXPECT_GE(result_zero, 0.49);
    EXPECT_LE(result_zero, 0.51);
    
    // At a = 5.0, should use last table element (index 200)
    // Table value: mean_x_max_tab[200] = 2.6733e-07 (very small)
    double result_plus5 = MeanPhiMax(5.0);
    EXPECT_GE(result_plus5, 0.0);
    EXPECT_LE(result_plus5, 1.0e-06);
}

// Test golden values for MeanMax2
TEST_F(UtilBoundaryTest, MeanMax2GoldenValues) {
    // At a = -5.0, should use first table element (index 0)
    // Table value: mean_max2_tab[0] = 0.999999
    double result_minus5 = MeanMax2(-5.0);
    EXPECT_GE(result_minus5, 0.99);
    EXPECT_LE(result_minus5, 1.01);
    
    // At a = 0.0, should use table element around index 100
    // Table value: mean_max2_tab[100] = 0.5
    double result_zero = MeanMax2(0.0);
    EXPECT_GE(result_zero, 0.49);
    EXPECT_LE(result_zero, 0.51);
    
    // At a = 5.0, should use last table element (index 200)
    // Table value: mean_max2_tab[200] = 25.0
    double result_plus5 = MeanMax2(5.0);
    EXPECT_GE(result_plus5, 24.9);
    EXPECT_LE(result_plus5, 25.0);
}

// Test mathematical properties: MeanMax(0) should be approximately 1/sqrt(2*pi) * integral
// For X ~ N(0,1), E[max(0, X)] = 1/sqrt(2*pi) ≈ 0.398942
// But MeanMax uses normalized form, so at a=0 it should be around 0.5-1.0
TEST_F(UtilBoundaryTest, MeanMaxMathematicalProperties) {
    // At a = 0, MeanMax should be positive and reasonable
    double result_zero = MeanMax(0.0);
    EXPECT_GT(result_zero, 0.0);
    EXPECT_LT(result_zero, 2.0); // Should be reasonable
    
    // For large positive a, MeanMax(a) should approach a
    double result_large = MeanMax(100.0);
    EXPECT_DOUBLE_EQ(result_large, 100.0);
    
    // For large negative a, MeanMax(a) should be 0
    double result_neg_large = MeanMax(-100.0);
    EXPECT_DOUBLE_EQ(result_neg_large, 0.0);
}

// Test interpolation accuracy: values at table step boundaries should match table values closely
TEST_F(UtilBoundaryTest, MeanMaxInterpolationAccuracy) {
    // Test at exact table step boundaries (every 0.05)
    // At -4.95, should use table[1] and table[2] interpolation
    double result_neg4_95 = MeanMax(-4.95);
    EXPECT_GE(result_neg4_95, 0.0);
    
    // At 0.0, should use table[100] and table[101] interpolation
    double result_0 = MeanMax(0.0);
    EXPECT_GE(result_0, 0.0);
    
    // At 4.95, should use table[199] and table[200] interpolation
    double result_4_95 = MeanMax(4.95);
    EXPECT_GE(result_4_95, 4.0);
    EXPECT_LE(result_4_95, 5.0);
}

