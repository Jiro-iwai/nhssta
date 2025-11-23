#include <gtest/gtest.h>
#include "../src/add.hpp"
#include "../src/normal.hpp"
#include <cmath>

using RandomVar = RandomVariable::RandomVariable;
using Normal = RandomVariable::Normal;

class AddTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Setup code if needed
    }

    void TearDown() override {
        // Cleanup code if needed
    }
};

// Test addition of two Normal random variables
TEST_F(AddTest, AddTwoNormals) {
    Normal a(10.0, 4.0);
    Normal b(5.0, 1.0);
    
    RandomVar sum = a + b;
    
    // Mean should be sum of means
    EXPECT_DOUBLE_EQ(sum->mean(), 15.0);
    
    // Variance should be sum of variances (when independent, covariance = 0)
    EXPECT_DOUBLE_EQ(sum->variance(), 5.0);
}

// Test addition with zero mean
TEST_F(AddTest, AddWithZeroMean) {
    Normal a(0.0, 1.0);
    Normal b(0.0, 1.0);
    
    RandomVar sum = a + b;
    
    EXPECT_DOUBLE_EQ(sum->mean(), 0.0);
    EXPECT_DOUBLE_EQ(sum->variance(), 2.0);
}

// Test addition with different variances
TEST_F(AddTest, AddDifferentVariances) {
    Normal a(10.0, 9.0);
    Normal b(20.0, 16.0);
    
    RandomVar sum = a + b;
    
    EXPECT_DOUBLE_EQ(sum->mean(), 30.0);
    EXPECT_DOUBLE_EQ(sum->variance(), 25.0);
}

// Test addition of same variable (should have covariance)
TEST_F(AddTest, AddSameVariable) {
    Normal a(10.0, 4.0);
    
    RandomVar sum = a + a;
    
    EXPECT_DOUBLE_EQ(sum->mean(), 20.0);
    // Variance should be 4 * variance of a (since covariance = variance when same)
    EXPECT_DOUBLE_EQ(sum->variance(), 16.0);
}

// Test chained addition
TEST_F(AddTest, ChainedAddition) {
    Normal a(1.0, 1.0);
    Normal b(2.0, 1.0);
    Normal c(3.0, 1.0);
    
    RandomVar sum = a + b + c;
    
    EXPECT_DOUBLE_EQ(sum->mean(), 6.0);
    // When independent, variance is sum
    EXPECT_DOUBLE_EQ(sum->variance(), 3.0);
}

