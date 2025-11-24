// -*- c++ -*-
// Unit tests for numerical error metrics in RandomVariable
// Issue #52: 機能追加: 数値誤差の規模を推定するメトリクスの追加

#include <gtest/gtest.h>

#include <cmath>
#include <limits>

#include "../src/add.hpp"
#include "../src/max.hpp"
#include "../src/normal.hpp"
#include "../src/random_variable.hpp"

using RandomVar = RandomVariable::RandomVariable;
using Normal = RandomVariable::Normal;

class NumericalErrorMetricsTest : public ::testing::Test {
   protected:
    void SetUp() override {}

    void TearDown() override {}
};

// Test: Standard deviation (sqrt(variance)) should be available
// This is a basic metric for numerical error estimation
TEST_F(NumericalErrorMetricsTest, StandardDeviation) {
    Normal n1(10.0, 4.0);  // mean=10, variance=4, stddev=2

    // Standard deviation should be sqrt(variance)
    double variance = n1->variance();
    double expected_stddev = std::sqrt(variance);

    // We expect a method to get standard deviation
    EXPECT_DOUBLE_EQ(expected_stddev, 2.0);

    // Test the standard_deviation() method
    EXPECT_DOUBLE_EQ(n1->standard_deviation(), 2.0);
}

// Test: Coefficient of variation (stddev / mean) for relative error estimation
// This metric helps understand error magnitude relative to the mean value
TEST_F(NumericalErrorMetricsTest, CoefficientOfVariation) {
    Normal n1(10.0, 4.0);   // mean=10, variance=4, stddev=2
    Normal n2(100.0, 4.0);  // mean=100, variance=4, stddev=2

    double stddev1 = std::sqrt(n1->variance());
    double stddev2 = std::sqrt(n2->variance());

    // Coefficient of variation = stddev / mean
    double cv1 = stddev1 / n1->mean();
    double cv2 = stddev2 / n2->mean();

    // n1 has higher relative error (20% vs 2%)
    EXPECT_GT(cv1, cv2);
    EXPECT_DOUBLE_EQ(cv1, 0.2);   // 2/10 = 0.2
    EXPECT_DOUBLE_EQ(cv2, 0.02);  // 2/100 = 0.02

    // Test the coefficient_of_variation() method
    EXPECT_DOUBLE_EQ(n1->coefficient_of_variation(), 0.2);
    EXPECT_DOUBLE_EQ(n2->coefficient_of_variation(), 0.02);

    // Test the relative_error() alias
    EXPECT_DOUBLE_EQ(n1->relative_error(), n1->coefficient_of_variation());
}

// Test: Relative error estimation for compound operations
// When combining RandomVariables, error propagation should be trackable
TEST_F(NumericalErrorMetricsTest, RelativeErrorInAddition) {
    Normal n1(10.0, 1.0);  // mean=10, variance=1, stddev=1
    Normal n2(20.0, 4.0);  // mean=20, variance=4, stddev=2

    using namespace RandomVariable;
    RandomVar sum = operator+(n1, n2);

    double sum_mean = sum->mean();
    double sum_variance = sum->variance();
    double sum_stddev = std::sqrt(sum_variance);

    // Sum: mean=30, variance=5 (assuming independent), stddev≈2.236
    EXPECT_DOUBLE_EQ(sum_mean, 30.0);
    EXPECT_GT(sum_variance, 4.0);  // At least n2's variance
    EXPECT_LT(sum_variance, 6.0);  // Less than sum of variances if correlated

    // Relative error = stddev / mean
    double relative_error = sum_stddev / sum_mean;
    EXPECT_GT(relative_error, 0.0);
    EXPECT_LT(relative_error, 1.0);  // Should be reasonable

    // Test the relative_error() method
    EXPECT_DOUBLE_EQ(sum->relative_error(), relative_error);
    EXPECT_DOUBLE_EQ(sum->standard_deviation(), sum_stddev);
}

// Test: Error propagation in MAX operation
// MAX operations can amplify errors, so tracking is important
TEST_F(NumericalErrorMetricsTest, ErrorPropagationInMax) {
    Normal n1(10.0, 1.0);  // mean=10, variance=1
    Normal n2(12.0, 1.0);  // mean=12, variance=1

    using namespace RandomVariable;
    RandomVar max_rv = MAX(n1, n2);

    double max_mean = max_rv->mean();
    double max_variance = max_rv->variance();
    double max_stddev = std::sqrt(max_variance);

    // MAX should be >= both inputs
    EXPECT_GE(max_mean, 10.0);
    EXPECT_GE(max_mean, 12.0);

    // Variance in MAX can be larger than input variances
    EXPECT_GT(max_variance, 0.0);

    // Relative error should be reasonable
    double relative_error = max_stddev / max_mean;
    EXPECT_GT(relative_error, 0.0);
    EXPECT_LT(relative_error, 1.0);

    // Test the relative_error() method
    EXPECT_DOUBLE_EQ(max_rv->relative_error(), relative_error);
    EXPECT_DOUBLE_EQ(max_rv->standard_deviation(), max_stddev);
}

// Test: Error metrics for edge cases (very small variance)
// Small variances can lead to numerical precision issues
TEST_F(NumericalErrorMetricsTest, ErrorMetricsForSmallVariance) {
    Normal n1(1.0, 1.0e-10);  // Very small variance

    double variance = n1->variance();
    double stddev = std::sqrt(variance);

    // Standard deviation should be very small but positive
    EXPECT_GT(stddev, 0.0);
    EXPECT_LE(stddev, 1.0e-5);  // sqrt(1e-10) ≈ 1e-5

    // Coefficient of variation might be very small
    double cv = stddev / n1->mean();
    EXPECT_GT(cv, 0.0);
    EXPECT_LE(cv, 1.0e-5);  // sqrt(1e-10) / 1.0 ≈ 1e-5

    // Test methods for edge cases
    EXPECT_DOUBLE_EQ(n1->standard_deviation(), stddev);
    EXPECT_DOUBLE_EQ(n1->coefficient_of_variation(), cv);
}

// Test: Error metrics for large values
// Large values might have different error characteristics
TEST_F(NumericalErrorMetricsTest, ErrorMetricsForLargeValues) {
    Normal n1(1.0e6, 1.0e4);  // mean=1M, variance=10K, stddev=100

    double variance = n1->variance();
    double stddev = std::sqrt(variance);
    double cv = stddev / n1->mean();

    // Relative error should be small for large values with reasonable variance
    EXPECT_GT(cv, 0.0);
    EXPECT_LT(cv, 0.1);  // Less than 10% relative error

    // Test methods for large value handling
    EXPECT_DOUBLE_EQ(n1->standard_deviation(), stddev);
    EXPECT_DOUBLE_EQ(n1->coefficient_of_variation(), cv);
}

// Test: Error metrics should be available for all RandomVariable types
// Consistency across different operation types
TEST_F(NumericalErrorMetricsTest, ErrorMetricsConsistency) {
    Normal n1(10.0, 4.0);
    Normal n2(20.0, 9.0);

    using namespace RandomVariable;
    RandomVar add_rv = operator+(n1, n2);
    RandomVar max_rv = MAX(n1, n2);

    // Both should have calculable error metrics
    double add_stddev = std::sqrt(add_rv->variance());
    double max_stddev = std::sqrt(max_rv->variance());

    EXPECT_GT(add_stddev, 0.0);
    EXPECT_GT(max_stddev, 0.0);

    // Ensure all RandomVariable types support error metrics
    EXPECT_NO_THROW((void)add_rv->standard_deviation());
    EXPECT_NO_THROW((void)max_rv->standard_deviation());
    EXPECT_NO_THROW((void)add_rv->coefficient_of_variation());
    EXPECT_NO_THROW((void)max_rv->coefficient_of_variation());
    EXPECT_NO_THROW((void)add_rv->relative_error());
    EXPECT_NO_THROW((void)max_rv->relative_error());
}
