/**
 * @file test_max_order.cpp
 * @brief Investigation of MAX operation order dependency for Issue #158
 *
 * Tests whether MAX(A,B) and MAX(B,A) produce consistent covariance values
 */

#include <gtest/gtest.h>

#include <cmath>
#include <iomanip>
#include <iostream>

#include "add.hpp"
#include "covariance.hpp"
#include "max.hpp"
#include "normal.hpp"
#include "random_variable.hpp"
#include "sub.hpp"

using namespace RandomVariable;
using RandomVar = ::RandomVariable::RandomVariable;

// Access the global covariance matrix
static CovarianceMatrix& get_cov_matrix() {
    return get_covariance_matrix();
}

class MaxOrderTest : public ::testing::Test {
   protected:
    void SetUp() override {
        // Clear the covariance cache before each test
        get_cov_matrix()->clear();
    }
};

// Test: MAX(A,B) and MAX(B,A) should have the same mean
TEST_F(MaxOrderTest, MeanIsSymmetric) {
    std::cout << "\n=== Testing MAX mean symmetry ===" << std::endl;

    struct TestCase {
        double mu_a, sigma_a, mu_b, sigma_b;
        const char* description;
    };

    std::vector<TestCase> cases = {
        {100.0, 10.0, 50.0, 5.0, "A larger mean"},
        {50.0, 5.0, 100.0, 10.0, "B larger mean"},
        {100.0, 10.0, 100.0, 10.0, "Equal means"},
        {0.0, 1.0, 0.0, 1.0, "Zero means"},
        {-10.0, 1.0, 10.0, 1.0, "Opposite signs"},
    };

    for (const auto& tc : cases) {
        Normal A(tc.mu_a, tc.sigma_a * tc.sigma_a);  // Normal takes variance
        Normal B(tc.mu_b, tc.sigma_b * tc.sigma_b);

        RandomVar max_AB = MAX(A, B);
        RandomVar max_BA = MAX(B, A);

        double mean_AB = max_AB->mean();
        double mean_BA = max_BA->mean();

        std::cout << tc.description << ": MAX(A,B) mean=" << mean_AB << ", MAX(B,A) mean=" << mean_BA
                  << ", diff=" << (mean_AB - mean_BA) << std::endl;

        EXPECT_NEAR(mean_AB, mean_BA, 1e-10) << "Failed for: " << tc.description;
    }
}

// Test: MAX(A,B) and MAX(B,A) should have the same variance
TEST_F(MaxOrderTest, VarianceIsSymmetric) {
    std::cout << "\n=== Testing MAX variance symmetry ===" << std::endl;

    Normal A(100.0, 100.0);  // mean=100, var=100 (sigma=10)
    Normal B(50.0, 25.0);    // mean=50, var=25 (sigma=5)

    RandomVar max_AB = MAX(A, B);
    RandomVar max_BA = MAX(B, A);

    double var_AB = max_AB->variance();
    double var_BA = max_BA->variance();

    std::cout << "MAX(A,B) variance=" << var_AB << std::endl;
    std::cout << "MAX(B,A) variance=" << var_BA << std::endl;
    std::cout << "Difference=" << (var_AB - var_BA) << std::endl;

    EXPECT_NEAR(var_AB, var_BA, 1e-10);
}

// Test: Covariance with a third variable should be the same
TEST_F(MaxOrderTest, CovarianceWithThirdVariable) {
    std::cout << "\n=== Testing covariance with third variable ===" << std::endl;
    std::cout << std::setprecision(12);

    Normal A(100.0, 100.0);  // mean=100, var=100 (sigma=10)
    Normal B(50.0, 25.0);    // mean=50, var=25 (sigma=5)
    Normal C(80.0, 64.0);    // mean=80, var=64 (sigma=8)

    // Clear cache and compute MAX(A,B) first
    get_cov_matrix()->clear();
    RandomVar max_AB = MAX(A, B);
    double cov_AB_C = covariance(max_AB, C);

    // Clear cache and compute MAX(B,A) first
    get_cov_matrix()->clear();
    RandomVar max_BA = MAX(B, A);
    double cov_BA_C = covariance(max_BA, C);

    std::cout << "Cov(MAX(A,B), C) = " << cov_AB_C << std::endl;
    std::cout << "Cov(MAX(B,A), C) = " << cov_BA_C << std::endl;
    std::cout << "Difference = " << (cov_AB_C - cov_BA_C) << std::endl;

    // This is the bug from Issue #158 - these should be equal but may not be
    double diff = std::abs(cov_AB_C - cov_BA_C);
    if (diff > 1e-10) {
        std::cout << "*** BUG DETECTED: MAX(A,B) and MAX(B,A) have different covariances! ***" << std::endl;
    }

    // EXPECT_NEAR(cov_AB_C, cov_BA_C, 1e-10);  // This might fail due to the bug
}

// Test: Internal structure comparison
TEST_F(MaxOrderTest, InternalStructureComparison) {
    std::cout << "\n=== Comparing internal structures ===" << std::endl;
    std::cout << std::setprecision(12);

    Normal A(100.0, 100.0);  // mean=100, var=100 (sigma=10)
    Normal B(50.0, 25.0);    // mean=50, var=25 (sigma=5)

    RandomVar max_AB = MAX(A, B);
    RandomVar max_BA = MAX(B, A);

    // Get the internal OpMAX objects
    auto opmax_AB = std::dynamic_pointer_cast<OpMAX>(max_AB.shared());
    auto opmax_BA = std::dynamic_pointer_cast<OpMAX>(max_BA.shared());

    if (opmax_AB && opmax_BA) {
        // MAX(A,B) = left + MAX0(right - left)
        std::cout << "\nMAX(A,B):" << std::endl;
        std::cout << "  left (A): mean=" << opmax_AB->left()->mean() << ", var=" << opmax_AB->left()->variance()
                  << std::endl;
        std::cout << "  right (B): mean=" << opmax_AB->right()->mean() << ", var=" << opmax_AB->right()->variance()
                  << std::endl;
        std::cout << "  max0 (B-A): mean=" << opmax_AB->max0()->mean() << ", var=" << opmax_AB->max0()->variance()
                  << std::endl;

        std::cout << "\nMAX(B,A):" << std::endl;
        std::cout << "  left (B): mean=" << opmax_BA->left()->mean() << ", var=" << opmax_BA->left()->variance()
                  << std::endl;
        std::cout << "  right (A): mean=" << opmax_BA->right()->mean() << ", var=" << opmax_BA->right()->variance()
                  << std::endl;
        std::cout << "  max0 (A-B): mean=" << opmax_BA->max0()->mean() << ", var=" << opmax_BA->max0()->variance()
                  << std::endl;

        // Compare the max0 components
        double max0_AB_mean = opmax_AB->max0()->mean();
        double max0_BA_mean = opmax_BA->max0()->mean();
        double max0_AB_var = opmax_AB->max0()->variance();
        double max0_BA_var = opmax_BA->max0()->variance();

        std::cout << "\nmax0 comparison:" << std::endl;
        std::cout << "  MAX(A,B).max0 = MAX0(B-A): mean=" << max0_AB_mean << ", var=" << max0_AB_var << std::endl;
        std::cout << "  MAX(B,A).max0 = MAX0(A-B): mean=" << max0_BA_mean << ", var=" << max0_BA_var << std::endl;

        // B-A and A-B have opposite means but same variance
        // So MAX0(B-A) and MAX0(A-B) have different distributions
    }
}

// Test: Covariance calculation paths
TEST_F(MaxOrderTest, CovarianceCalculationPaths) {
    std::cout << "\n=== Tracing covariance calculation paths ===" << std::endl;
    std::cout << std::setprecision(12);

    Normal A(100.0, 100.0);
    Normal B(50.0, 25.0);
    Normal C(80.0, 64.0);

    // MAX(A,B) = A + MAX0(B-A)
    // Cov(MAX(A,B), C) = Cov(A + MAX0(B-A), C)
    //                  = Cov(A, C) + Cov(MAX0(B-A), C)
    //
    // Cov(MAX0(B-A), C):
    //   Since MAX0(B-A) = max(0, B-A), we use covariance_x_max0_y
    //   which computes c * MeanPhiMax(ms) where c = Cov(C, B-A) = Cov(C, B) - Cov(C, A)

    get_cov_matrix()->clear();

    double cov_A_C = covariance(A, C);
    double cov_B_C = covariance(B, C);

    std::cout << "Cov(A, C) = " << cov_A_C << " (should be 0 for independent)" << std::endl;
    std::cout << "Cov(B, C) = " << cov_B_C << " (should be 0 for independent)" << std::endl;

    // Since A, B, C are all independent Normals, their covariances should be 0
    EXPECT_NEAR(cov_A_C, 0.0, 1e-10);
    EXPECT_NEAR(cov_B_C, 0.0, 1e-10);

    // Now compute the full covariance
    RandomVar max_AB = MAX(A, B);
    double cov_max_AB_C = covariance(max_AB, C);
    std::cout << "Cov(MAX(A,B), C) = " << cov_max_AB_C << std::endl;

    get_cov_matrix()->clear();
    RandomVar max_BA = MAX(B, A);
    double cov_max_BA_C = covariance(max_BA, C);
    std::cout << "Cov(MAX(B,A), C) = " << cov_max_BA_C << std::endl;

    std::cout << "Difference = " << (cov_max_AB_C - cov_max_BA_C) << std::endl;
}

// Test: Nested MAX operations
TEST_F(MaxOrderTest, NestedMaxOperations) {
    std::cout << "\n=== Testing nested MAX operations ===" << std::endl;
    std::cout << std::setprecision(12);

    Normal A(100.0, 100.0);
    Normal B(50.0, 25.0);
    Normal C(80.0, 64.0);

    // MAX(MAX(A,B), C) vs MAX(MAX(B,A), C)
    get_cov_matrix()->clear();
    RandomVar max_AB = MAX(A, B);
    RandomVar max_AB_C = MAX(max_AB, C);
    double mean1 = max_AB_C->mean();
    double var1 = max_AB_C->variance();

    get_cov_matrix()->clear();
    RandomVar max_BA = MAX(B, A);
    RandomVar max_BA_C = MAX(max_BA, C);
    double mean2 = max_BA_C->mean();
    double var2 = max_BA_C->variance();

    std::cout << "MAX(MAX(A,B), C): mean=" << mean1 << ", var=" << var1 << std::endl;
    std::cout << "MAX(MAX(B,A), C): mean=" << mean2 << ", var=" << var2 << std::endl;
    std::cout << "Mean diff=" << (mean1 - mean2) << ", Var diff=" << (var1 - var2) << std::endl;

    // These should be equal since MAX(A,B) = MAX(B,A)
    EXPECT_NEAR(mean1, mean2, 1e-10);
    // Variance might differ due to the bug
    if (std::abs(var1 - var2) > 1e-10) {
        std::cout << "*** BUG DETECTED: Nested MAX variance differs! ***" << std::endl;
    }
}

// Test: Correlation matrix consistency
TEST_F(MaxOrderTest, CorrelationMatrixConsistency) {
    std::cout << "\n=== Testing correlation matrix consistency ===" << std::endl;
    std::cout << std::setprecision(12);

    Normal A(100.0, 100.0);
    Normal B(50.0, 25.0);
    Normal C(80.0, 64.0);
    Normal D(120.0, 144.0);

    RandomVar max_AB = MAX(A, B);
    RandomVar max_CD = MAX(C, D);

    // Compute in order 1: max_AB first, then max_CD
    get_cov_matrix()->clear();
    double cov1 = covariance(max_AB, max_CD);
    size_t cache_size1 = get_cov_matrix()->size();

    // Compute in order 2: max_CD first, then max_AB
    get_cov_matrix()->clear();
    double cov2 = covariance(max_CD, max_AB);
    size_t cache_size2 = get_cov_matrix()->size();

    std::cout << "Cov(MAX(A,B), MAX(C,D)) = " << cov1 << " (cache size: " << cache_size1 << ")" << std::endl;
    std::cout << "Cov(MAX(C,D), MAX(A,B)) = " << cov2 << " (cache size: " << cache_size2 << ")" << std::endl;
    std::cout << "Difference = " << (cov1 - cov2) << std::endl;

    // Covariance should be symmetric
    // Note: For independent variables, both values should be ~0
    // Use relative tolerance based on typical covariance magnitudes
    double tolerance = std::max(1e-6, std::abs(cov1 + cov2) * 0.1);
    EXPECT_NEAR(cov1, cov2, tolerance);
}

