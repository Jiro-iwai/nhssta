#include <gtest/gtest.h>

#include <cmath>

#include "../src/add.hpp"
#include "../src/normal.hpp"
#include "../src/random_variable.hpp"

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

// Test const correctness: mean() and variance() should be callable on const RandomVariableImpl&
// This tests the actual const-correctness of the implementation class methods
TEST_F(RandomVariableTest, ConstCorrectnessImplMeanVariance) {
    RandomVariable::Normal n(10.0, 4.0);
    
    // Get a const reference to the underlying implementation
    const RandomVariable::RandomVariableImpl& const_impl = *n.get();
    
    // These should compile and work correctly on const reference to implementation
    EXPECT_DOUBLE_EQ(const_impl.mean(), 10.0);
    EXPECT_DOUBLE_EQ(const_impl.variance(), 4.0);
}

// Test that const reference works with computed values (lazy evaluation)
TEST_F(RandomVariableTest, ConstCorrectnessWithLazyEvaluation) {
    RandomVariable::Normal a(3.0, 1.0);
    RandomVariable::Normal b(4.0, 1.0);
    RandomVar sum = a + b;
    
    // Get a const reference to the underlying implementation
    const RandomVariable::RandomVariableImpl& const_impl = *sum.get();
    
    // mean() and variance() should work on const reference even with lazy evaluation
    EXPECT_DOUBLE_EQ(const_impl.mean(), 7.0);
    EXPECT_DOUBLE_EQ(const_impl.variance(), 2.0);
}

// Test that standard_deviation and coefficient_of_variation are also const-correct
TEST_F(RandomVariableTest, ConstCorrectnessStatMethods) {
    RandomVariable::Normal n(10.0, 4.0);
    
    // Get a const reference to the underlying implementation
    const RandomVariable::RandomVariableImpl& const_impl = *n.get();
    
    // These derived methods should also work on const references
    EXPECT_DOUBLE_EQ(const_impl.standard_deviation(), 2.0);
    EXPECT_NEAR(const_impl.coefficient_of_variation(), 0.2, 1e-10);
    EXPECT_NEAR(const_impl.relative_error(), 0.2, 1e-10);
}
