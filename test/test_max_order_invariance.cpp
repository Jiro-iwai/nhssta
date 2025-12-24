/**
 * @file test_max_order_invariance.cpp
 * @brief Tests for MAX operation order invariance (sym_mod_2 implementation)
 *
 * After sym_mod_2 implementation, MAX(A,B) and MAX(B,A) should produce
 * identical results for mean, variance, and covariance computations.
 */

#include <gtest/gtest.h>

#include <cmath>
#include <iomanip>
#include <iostream>

#include "covariance.hpp"
#include "max.hpp"
#include "normal.hpp"

using namespace RandomVariable;

class MaxOrderInvarianceTest : public ::testing::Test {
protected:
    void SetUp() override {
        get_covariance_matrix()->clear();
    }

    void TearDown() override {
        get_covariance_matrix()->clear();
    }
};

// Test 4.1: MAX itself is order-invariant (mean and variance)
TEST_F(MaxOrderInvarianceTest, MaxMeanVarianceOrderInvariant) {
    // Test cases with various mean differences
    struct TestCase {
        double mu_x, sigma_x, mu_y, sigma_y;
        const char* description;
    };

    std::vector<TestCase> cases = {
        {10.0, 2.0, 5.0, 1.5, "X > Y (different means)"},
        {5.0, 1.5, 10.0, 2.0, "Y > X (reversed)"},
        {8.0, 2.0, 8.0, 2.0, "X == Y (equal means)"},
        {8.0, 2.0, 8.001, 2.0, "X ≈ Y (very close)"},
        {0.0, 1.0, 0.0, 1.0, "Both zero mean"},
    };

    for (const auto& tc : cases) {
        Normal X(tc.mu_x, tc.sigma_x * tc.sigma_x);
        Normal Y(tc.mu_y, tc.sigma_y * tc.sigma_y);

        auto max_XY = MAX(X, Y);
        auto max_YX = MAX(Y, X);

        double mean_XY = max_XY->mean();
        double mean_YX = max_YX->mean();
        double var_XY = max_XY->variance();
        double var_YX = max_YX->variance();

        EXPECT_NEAR(mean_XY, mean_YX, 1e-12)
            << "Case: " << tc.description << " - mean should be order-invariant";
        EXPECT_NEAR(var_XY, var_YX, 1e-12)
            << "Case: " << tc.description << " - variance should be order-invariant";

        std::cout << tc.description << ": mean=" << mean_XY << ", var=" << var_XY << std::endl;
    }
}

// Test 4.2: Cov(Gaussian, MAX) is order-invariant
TEST_F(MaxOrderInvarianceTest, CovGaussianMaxOrderInvariant) {
    Normal W(5.0, 4.0);   // μ=5, σ²=4
    Normal X(10.0, 9.0);  // μ=10, σ²=9
    Normal Y(8.0, 4.0);   // μ=8, σ²=4

    // Set some correlation
    get_covariance_matrix()->set(W, X, 2.0);
    get_covariance_matrix()->set(W, Y, 1.5);

    auto max_XY = MAX(X, Y);
    auto max_YX = MAX(Y, X);

    double cov_W_maxXY = covariance(W, max_XY);
    double cov_W_maxYX = covariance(W, max_YX);
    double cov_maxXY_W = covariance(max_XY, W);
    double cov_maxYX_W = covariance(max_YX, W);

    EXPECT_NEAR(cov_W_maxXY, cov_W_maxYX, 1e-12)
        << "Cov(W, MAX(X,Y)) should equal Cov(W, MAX(Y,X))";
    EXPECT_NEAR(cov_maxXY_W, cov_maxYX_W, 1e-12)
        << "Cov(MAX(X,Y), W) should equal Cov(MAX(Y,X), W)";
    EXPECT_NEAR(cov_W_maxXY, cov_maxXY_W, 1e-12)
        << "Covariance should be symmetric";

    std::cout << "Cov(W, MAX(X,Y)) = " << cov_W_maxXY << std::endl;
    std::cout << "Cov(W, MAX(Y,X)) = " << cov_W_maxYX << std::endl;
}

// Test 4.3: Cov(MAX, MAX) is order-invariant (most critical)
TEST_F(MaxOrderInvarianceTest, CovMaxMaxOrderInvariant) {
    Normal X1(10.0, 4.0);  // μ=10, σ²=4
    Normal Y1(8.0, 2.0);   // μ=8, σ²=2
    Normal X2(12.0, 3.0);  // μ=12, σ²=3
    Normal Y2(9.0, 2.5);   // μ=9, σ²=2.5

    // Set some correlations
    get_covariance_matrix()->set(X1, Y1, 1.0);
    get_covariance_matrix()->set(X2, Y2, 0.8);
    get_covariance_matrix()->set(X1, X2, 1.2);
    get_covariance_matrix()->set(Y1, Y2, 0.5);

    // All 4 combinations
    auto max_X1Y1 = MAX(X1, Y1);
    auto max_Y1X1 = MAX(Y1, X1);
    auto max_X2Y2 = MAX(X2, Y2);
    auto max_Y2X2 = MAX(Y2, X2);

    double cov_1 = covariance(max_X1Y1, max_X2Y2);
    double cov_2 = covariance(max_Y1X1, max_X2Y2);
    double cov_3 = covariance(max_X1Y1, max_Y2X2);
    double cov_4 = covariance(max_Y1X1, max_Y2X2);

    EXPECT_NEAR(cov_1, cov_2, 1e-12)
        << "Cov(MAX(X1,Y1), MAX(X2,Y2)) should equal Cov(MAX(Y1,X1), MAX(X2,Y2))";
    EXPECT_NEAR(cov_1, cov_3, 1e-12)
        << "Cov(MAX(X1,Y1), MAX(X2,Y2)) should equal Cov(MAX(X1,Y1), MAX(Y2,X2))";
    EXPECT_NEAR(cov_1, cov_4, 1e-12)
        << "All orderings should produce the same covariance";

    std::cout << "Cov(MAX(X1,Y1), MAX(X2,Y2)) = " << cov_1 << std::endl;
    std::cout << "Cov(MAX(Y1,X1), MAX(X2,Y2)) = " << cov_2 << std::endl;
    std::cout << "Cov(MAX(X1,Y1), MAX(Y2,X2)) = " << cov_3 << std::endl;
    std::cout << "Cov(MAX(Y1,X1), MAX(Y2,X2)) = " << cov_4 << std::endl;
}

// Test 4.4: Multi-stage MAX order invariance
TEST_F(MaxOrderInvarianceTest, MultiStageMaxOrderInvariant) {
    Normal A(10.0, 4.0);  // μ=10, σ²=4
    Normal B(8.0, 1.0);   // μ=8, σ²=1
    Normal C(12.0, 2.0);  // μ=12, σ²=2

    // Set some correlations
    get_covariance_matrix()->set(A, B, 0.5);
    get_covariance_matrix()->set(A, C, 0.8);
    get_covariance_matrix()->set(B, C, 0.3);

    // Different orderings of multi-stage MAX
    auto result1 = MAX(MAX(A, B), MAX(A, C));
    auto result2 = MAX(MAX(A, C), MAX(A, B));
    auto result3 = MAX(MAX(B, A), MAX(C, A));
    auto result4 = MAX(MAX(C, A), MAX(B, A));

    double mean1 = result1->mean();
    double mean2 = result2->mean();
    double mean3 = result3->mean();
    double mean4 = result4->mean();

    double var1 = result1->variance();
    double var2 = result2->variance();
    double var3 = result3->variance();
    double var4 = result4->variance();

    EXPECT_NEAR(mean1, mean2, 1e-12) << "Multi-stage MAX mean should be order-invariant (1 vs 2)";
    EXPECT_NEAR(mean1, mean3, 1e-12) << "Multi-stage MAX mean should be order-invariant (1 vs 3)";
    EXPECT_NEAR(mean1, mean4, 1e-12) << "Multi-stage MAX mean should be order-invariant (1 vs 4)";

    EXPECT_NEAR(var1, var2, 1e-12) << "Multi-stage MAX variance should be order-invariant (1 vs 2)";
    EXPECT_NEAR(var1, var3, 1e-12) << "Multi-stage MAX variance should be order-invariant (1 vs 3)";
    EXPECT_NEAR(var1, var4, 1e-12) << "Multi-stage MAX variance should be order-invariant (1 vs 4)";

    std::cout << "Multi-stage MAX mean: " << mean1 << ", var: " << var1 << std::endl;
}

// Test: Covariance with multi-stage MAX
TEST_F(MaxOrderInvarianceTest, CovarianceWithMultiStageMax) {
    Normal A(10.0, 4.0);
    Normal B(8.0, 1.0);
    Normal C(12.0, 2.0);
    Normal W(5.0, 3.0);

    get_covariance_matrix()->set(A, B, 0.5);
    get_covariance_matrix()->set(A, C, 0.8);
    get_covariance_matrix()->set(A, W, 1.0);
    get_covariance_matrix()->set(B, W, 0.4);
    get_covariance_matrix()->set(C, W, 0.6);

    auto max1 = MAX(MAX(A, B), MAX(A, C));
    auto max2 = MAX(MAX(A, C), MAX(A, B));

    double cov1 = covariance(W, max1);
    double cov2 = covariance(W, max2);

    EXPECT_NEAR(cov1, cov2, 1e-12)
        << "Cov(W, multi-stage-MAX) should be order-invariant";

    std::cout << "Cov(W, MAX(MAX(A,B), MAX(A,C))) = " << cov1 << std::endl;
    std::cout << "Cov(W, MAX(MAX(A,C), MAX(A,B))) = " << cov2 << std::endl;
}
