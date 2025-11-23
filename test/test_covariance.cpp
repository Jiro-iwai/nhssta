#include <gtest/gtest.h>
#include "../src/Covariance.h"
#include "../src/Normal.h"
#include "../src/ADD.h"
#include <cmath>

using namespace RandomVariable;

class CovarianceTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Clear covariance matrix before each test
        get_covariance_matrix()->clear();
    }

    void TearDown() override {
        // Cleanup code if needed
    }
};

// Test covariance of same Normal variable
TEST_F(CovarianceTest, CovarianceSameVariable) {
    Normal a(10.0, 4.0);
    
    double cov = covariance(a, a);
    
    // Covariance of variable with itself should equal variance
    EXPECT_DOUBLE_EQ(cov, 4.0);
}

// Test covariance of two independent Normal variables
TEST_F(CovarianceTest, CovarianceIndependentVariables) {
    Normal a(10.0, 4.0);
    Normal b(5.0, 1.0);
    
    double cov = covariance(a, b);
    
    // Independent variables should have zero covariance
    EXPECT_DOUBLE_EQ(cov, 0.0);
}

// Test covariance of two different independent Normals
TEST_F(CovarianceTest, CovarianceDifferentIndependent) {
    Normal a(20.0, 9.0);
    Normal b(15.0, 16.0);
    
    double cov = covariance(a, b);
    
    EXPECT_DOUBLE_EQ(cov, 0.0);
}

// Test covariance with addition (should have non-zero covariance)
TEST_F(CovarianceTest, CovarianceWithAddition) {
    Normal a(10.0, 4.0);
    Normal b(5.0, 1.0);
    
    RandomVariable sum = a + b;
    
    // Covariance of a with (a+b) should be variance of a
    double cov_a_sum = covariance(a, sum);
    EXPECT_DOUBLE_EQ(cov_a_sum, 4.0);
    
    // Covariance of b with (a+b) should be variance of b
    double cov_b_sum = covariance(b, sum);
    EXPECT_DOUBLE_EQ(cov_b_sum, 1.0);
}

// Test covariance matrix caching
TEST_F(CovarianceTest, CovarianceMatrixCaching) {
    Normal a(10.0, 4.0);
    Normal b(5.0, 1.0);
    
    RandomVariable sum = a + b;
    
    // First call
    double cov1 = covariance(a, sum);
    
    // Second call should use cached value
    double cov2 = covariance(a, sum);
    
    EXPECT_DOUBLE_EQ(cov1, cov2);
}

// Test covariance symmetry
TEST_F(CovarianceTest, CovarianceSymmetry) {
    Normal a(10.0, 4.0);
    Normal b(5.0, 1.0);
    
    RandomVariable sum = a + b;
    
    double cov_ab = covariance(a, sum);
    double cov_ba = covariance(sum, a);
    
    EXPECT_DOUBLE_EQ(cov_ab, cov_ba);
}

