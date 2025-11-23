#include <gtest/gtest.h>
#include "../src/MAX.h"
#include "../src/Normal.h"
#include <cmath>

using RandomVar = RandomVariable::RandomVariable;
using Normal = RandomVariable::Normal;

class MaxTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Setup code if needed
    }

    void TearDown() override {
        // Cleanup code if needed
    }
};

// Test MAX of two independent Normal random variables
TEST_F(MaxTest, MaxTwoNormals) {
    Normal a(10.0, 4.0);
    Normal b(5.0, 1.0);
    
    RandomVar max = MAX(a, b);
    
    // MAX(a, b) = a + MAX0(b - a)
    // Since b - a = -5.0 (negative), MAX0 is approximately 0
    // So mean should be close to a's mean (10.0), but may be slightly higher
    double mean = max->mean();
    EXPECT_GE(mean, 9.5); // Should be >= a's mean (with some tolerance)
    EXPECT_LE(mean, 12.0); // Should be reasonable upper bound
    
    // Variance should be positive
    double variance = max->variance();
    EXPECT_GT(variance, 0.0);
}

// Test MAX when first variable is clearly larger
TEST_F(MaxTest, MaxWhenFirstLarger) {
    Normal a(20.0, 4.0);
    Normal b(5.0, 1.0);
    
    RandomVar max = MAX(a, b);
    
    // Mean should be close to 20.0 (since a is much larger)
    double mean = max->mean();
    EXPECT_GT(mean, 15.0);
    EXPECT_LT(mean, 25.0);
}

// Test MAX when second variable is clearly larger
TEST_F(MaxTest, MaxWhenSecondLarger) {
    Normal a(5.0, 1.0);
    Normal b(20.0, 4.0);
    
    RandomVar max = MAX(a, b);
    
    // Mean should be close to 20.0 (since b is much larger)
    double mean = max->mean();
    EXPECT_GT(mean, 15.0);
    EXPECT_LT(mean, 25.0);
}

// Test MAX with equal means
TEST_F(MaxTest, MaxEqualMeans) {
    Normal a(10.0, 4.0);
    Normal b(10.0, 4.0);
    
    RandomVar max = MAX(a, b);
    
    // Mean should be >= 10.0
    double mean = max->mean();
    EXPECT_GE(mean, 10.0);
    
    // Variance should be positive
    double variance = max->variance();
    EXPECT_GT(variance, 0.0);
}

// Test MAX0 (max with zero)
TEST_F(MaxTest, Max0Test) {
    Normal a(5.0, 4.0);
    
    RandomVar max0 = MAX0(a);
    
    // Mean should be >= 5.0 (since we're taking max with 0)
    double mean = max0->mean();
    EXPECT_GE(mean, 5.0);
    
    // Variance should be positive
    double variance = max0->variance();
    EXPECT_GT(variance, 0.0);
}

// Test MAX0 with negative mean
TEST_F(MaxTest, Max0NegativeMean) {
    Normal a(-5.0, 4.0);
    
    RandomVar max0 = MAX0(a);
    
    // Mean should be >= 0.0 (since we're taking max with 0)
    double mean = max0->mean();
    EXPECT_GE(mean, 0.0);
}

