#include <gtest/gtest.h>

#include <cmath>
#include <limits>

#include "../src/normal.hpp"
#include <nhssta/exception.hpp>

using namespace RandomVariable;

class NormalTest : public ::testing::Test {
   protected:
    void SetUp() override {
        // Setup code if needed
    }

    void TearDown() override {
        // Cleanup code if needed
    }
};

// Test Normal construction with mean and variance
TEST_F(NormalTest, BasicConstruction) {
    Normal n(10.0, 4.0);
    EXPECT_DOUBLE_EQ(n->mean(), 10.0);
    EXPECT_DOUBLE_EQ(n->variance(), 4.0);
}

// Test Normal construction with zero variance
TEST_F(NormalTest, ZeroVariance) {
    Normal n(5.0, 0.0);
    EXPECT_DOUBLE_EQ(n->mean(), 5.0);
    EXPECT_DOUBLE_EQ(n->variance(), 0.0);
}

// Test Normal clone
TEST_F(NormalTest, Clone) {
    Normal n1(10.0, 4.0);
    Normal n2 = n1.clone();

    EXPECT_DOUBLE_EQ(n2->mean(), n1->mean());
    EXPECT_DOUBLE_EQ(n2->variance(), n1->variance());

    // Cloned object should be independent
    EXPECT_NE(&(*n1), &(*n2));
}

// Test Normal with negative variance (should throw)
TEST_F(NormalTest, NegativeVarianceThrows) {
    EXPECT_THROW({ Normal n(10.0, -1.0); }, Nh::RuntimeException);
}

// Test Normal mean calculation
TEST_F(NormalTest, MeanCalculation) {
    Normal n(20.0, 16.0);
    EXPECT_DOUBLE_EQ(n->mean(), 20.0);
}

// Test Normal variance calculation
TEST_F(NormalTest, VarianceCalculation) {
    Normal n(20.0, 16.0);
    EXPECT_DOUBLE_EQ(n->variance(), 16.0);
}

// Test Normal with minimum variance
TEST_F(NormalTest, MinimumVariance) {
    using namespace RandomVariable;
    Normal n(0.0, MINIMUM_VARIANCE);
    EXPECT_DOUBLE_EQ(n->variance(), MINIMUM_VARIANCE);
}

// ============================================================
// NaN/Inf validation tests (Issue #136)
// These tests verify that invalid numeric values are caught
// at the input boundary to prevent silent propagation.
// ============================================================

// Test: NaN mean in constructor should throw
TEST_F(NormalTest, NaNMeanThrows) {
    double nan_value = std::nan("");
    EXPECT_THROW({ Normal n(nan_value, 1.0); }, Nh::RuntimeException);
}

// Test: NaN variance in constructor should throw
TEST_F(NormalTest, NaNVarianceThrows) {
    double nan_value = std::nan("");
    EXPECT_THROW({ Normal n(1.0, nan_value); }, Nh::RuntimeException);
}

// Test: Positive infinity mean in constructor should throw
TEST_F(NormalTest, InfMeanThrows) {
    double inf_value = std::numeric_limits<double>::infinity();
    EXPECT_THROW({ Normal n(inf_value, 1.0); }, Nh::RuntimeException);
}

// Test: Negative infinity mean in constructor should throw
TEST_F(NormalTest, NegInfMeanThrows) {
    double neg_inf_value = -std::numeric_limits<double>::infinity();
    EXPECT_THROW({ Normal n(neg_inf_value, 1.0); }, Nh::RuntimeException);
}

// Test: Positive infinity variance in constructor should throw
TEST_F(NormalTest, InfVarianceThrows) {
    double inf_value = std::numeric_limits<double>::infinity();
    EXPECT_THROW({ Normal n(1.0, inf_value); }, Nh::RuntimeException);
}

// Test: Valid operations don't produce NaN
TEST_F(NormalTest, ValidOperationsProduceValidResults) {
    // Create normal distributions with valid values
    Normal n1(10.0, 4.0);
    Normal n2(5.0, 1.0);

    // Verify results are not NaN
    EXPECT_FALSE(std::isnan(n1->mean()));
    EXPECT_FALSE(std::isnan(n1->variance()));
    EXPECT_FALSE(std::isnan(n1->standard_deviation()));

    EXPECT_FALSE(std::isnan(n2->mean()));
    EXPECT_FALSE(std::isnan(n2->variance()));
    EXPECT_FALSE(std::isnan(n2->standard_deviation()));
}

// Test: Coefficient of variation with zero mean returns infinity (not NaN)
TEST_F(NormalTest, CoefficientOfVariationZeroMean) {
    Normal n(0.0, 1.0);
    double cv = n->coefficient_of_variation();
    // Should return infinity, not NaN
    EXPECT_TRUE(std::isinf(cv));
    EXPECT_FALSE(std::isnan(cv));
}
