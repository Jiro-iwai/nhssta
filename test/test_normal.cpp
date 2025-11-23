#include <gtest/gtest.h>
#include "../src/Normal.h"
#include <cmath>

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
    Normal n2 = n1->clone();
    
    EXPECT_DOUBLE_EQ(n2->mean(), n1->mean());
    EXPECT_DOUBLE_EQ(n2->variance(), n1->variance());
    
    // Cloned object should be independent
    EXPECT_NE(&(*n1), &(*n2));
}

// Test Normal with negative variance (should throw)
TEST_F(NormalTest, NegativeVarianceThrows) {
    EXPECT_THROW({
        Normal n(10.0, -1.0);
    }, Exception);
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
    Normal n(0.0, minimum_variance);
    EXPECT_DOUBLE_EQ(n->variance(), minimum_variance);
}

