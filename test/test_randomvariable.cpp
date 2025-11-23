#include <gtest/gtest.h>
#include "../src/RandomVariable.h"
#include "../src/Normal.h"
#include "../src/ADD.h"
#include <cmath>

using RandomVar = RandomVariable::RandomVariable;

class RandomVariableTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Setup code if needed
    }

    void TearDown() override {
        // Cleanup code if needed
    }
};

// Test basic RandomVariable construction (using Normal as concrete implementation)
TEST_F(RandomVariableTest, BasicConstruction) {
    RandomVariable::Normal n(0.0, 0.0);
    RandomVar rv = n;
    EXPECT_EQ(rv->mean(), 0.0);
    EXPECT_EQ(rv->variance(), 0.0);
    EXPECT_EQ(rv->name(), "");
}

// Test RandomVariable with mean and variance
TEST_F(RandomVariableTest, ConstructionWithMeanVariance) {
    RandomVariable::Normal n(10.0, 4.0);
    RandomVar rv = n;
    EXPECT_DOUBLE_EQ(rv->mean(), 10.0);
    EXPECT_DOUBLE_EQ(rv->variance(), 4.0);
}

// Test name setting
TEST_F(RandomVariableTest, NameSetting) {
    RandomVariable::Normal n(5.0, 1.0);
    RandomVar rv = n;
    rv->set_name("test_var");
    EXPECT_EQ(rv->name(), "test_var");
}

// Test level property
TEST_F(RandomVariableTest, LevelProperty) {
    RandomVariable::Normal n(0.0, 1.0);
    RandomVar rv = n;
    EXPECT_EQ(rv->level(), 0);
}

// Test variance calculation
TEST_F(RandomVariableTest, VarianceCalculation) {
    RandomVariable::Normal n(10.0, 4.0);
    RandomVar rv = n;
    double variance = rv->variance();
    EXPECT_DOUBLE_EQ(variance, 4.0);
}

// Test mean calculation
TEST_F(RandomVariableTest, MeanCalculation) {
    RandomVariable::Normal n(15.0, 9.0);
    RandomVar rv = n;
    double mean = rv->mean();
    EXPECT_DOUBLE_EQ(mean, 15.0);
}

TEST_F(RandomVariableTest, CopySharesUnderlyingStorage) {
    RandomVariable::Normal n(2.0, 1.0);
    RandomVar rv1 = n;
    RandomVar rv2 = rv1;

    rv1->set_name("shared");
    EXPECT_EQ(rv2->name(), "shared");
    EXPECT_EQ(rv1.get(), rv2.get());
}

TEST_F(RandomVariableTest, OperationResultSurvivesSourceScope) {
    RandomVar sum;
    {
        RandomVariable::Normal a(3.0, 0.25);
        RandomVariable::Normal b(4.0, 0.25);
        sum = a + b;
    }

    EXPECT_DOUBLE_EQ(sum->mean(), 7.0);
    EXPECT_DOUBLE_EQ(sum->variance(), 0.5);
}

