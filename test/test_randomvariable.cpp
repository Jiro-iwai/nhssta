#include <gtest/gtest.h>
#include "../src/RandomVariable.h"
#include <cmath>

using namespace RandomVariable;

class RandomVariableTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Setup code if needed
    }

    void TearDown() override {
        // Cleanup code if needed
    }
};

// Test basic RandomVariable construction
TEST_F(RandomVariableTest, BasicConstruction) {
    RandomVariable rv;
    EXPECT_EQ(rv->mean(), 0.0);
    EXPECT_EQ(rv->variance(), 0.0);
    EXPECT_EQ(rv->name(), "");
}

// Test RandomVariable with mean and variance
TEST_F(RandomVariableTest, ConstructionWithMeanVariance) {
    RandomVariable rv(10.0, 4.0);
    EXPECT_DOUBLE_EQ(rv->mean(), 10.0);
    EXPECT_DOUBLE_EQ(rv->variance(), 4.0);
}

// Test name setting
TEST_F(RandomVariableTest, NameSetting) {
    RandomVariable rv(5.0, 1.0);
    rv->set_name("test_var");
    EXPECT_EQ(rv->name(), "test_var");
}

// Test level property
TEST_F(RandomVariableTest, LevelProperty) {
    RandomVariable rv(0.0, 1.0);
    EXPECT_EQ(rv->level(), 0);
}

// Test variance calculation
TEST_F(RandomVariableTest, VarianceCalculation) {
    RandomVariable rv(10.0, 4.0);
    double variance = rv->variance();
    EXPECT_DOUBLE_EQ(variance, 4.0);
}

// Test mean calculation
TEST_F(RandomVariableTest, MeanCalculation) {
    RandomVariable rv(15.0, 9.0);
    double mean = rv->mean();
    EXPECT_DOUBLE_EQ(mean, 15.0);
}

