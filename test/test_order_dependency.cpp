/**
 * @file test_order_dependency.cpp
 * @brief Test for Issue #159: Covariance cache order dependency
 *
 * Verifies that covariance calculations give the same results regardless
 * of the order in which they are computed.
 */

#include <gtest/gtest.h>

#include <cmath>
#include <iomanip>
#include <iostream>
#include <vector>

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

class OrderDependencyTest : public ::testing::Test {
   protected:
    void SetUp() override {
        get_cov_matrix()->clear();
    }
};

// Test: Covariance calculation order should not affect results
TEST_F(OrderDependencyTest, CovarianceOrderIndependence) {
    std::cout << "\n=== Testing covariance order independence ===" << std::endl;
    std::cout << std::setprecision(12);

    // Create a simple circuit-like structure
    Normal A(100.0, 100.0);  // Gate A delay
    Normal B(80.0, 64.0);    // Gate B delay
    Normal C(120.0, 144.0);  // Gate C delay
    Normal D(90.0, 81.0);    // Gate D delay

    // MAX operations (simulating critical path selection)
    RandomVar max_AB = MAX(A, B);
    RandomVar max_CD = MAX(C, D);
    RandomVar max_final = MAX(max_AB, max_CD);

    // Order 1: Compute full correlation matrix first, then specific pairs
    get_cov_matrix()->clear();
    std::vector<std::pair<RandomVar, RandomVar>> pairs1 = {
        {A, B}, {A, C}, {A, D}, {B, C}, {B, D}, {C, D},
        {max_AB, A}, {max_AB, B}, {max_AB, C}, {max_AB, D},
        {max_CD, A}, {max_CD, B}, {max_CD, C}, {max_CD, D},
        {max_AB, max_CD}, {max_final, A}, {max_final, max_AB}
    };
    std::vector<double> results1;
    for (const auto& p : pairs1) {
        results1.push_back(covariance(p.first, p.second));
    }
    size_t cache1 = get_cov_matrix()->size();

    // Order 2: Compute only specific pairs directly
    get_cov_matrix()->clear();
    double cov_max_AB_max_CD_direct = covariance(max_AB, max_CD);
    double cov_max_final_A_direct = covariance(max_final, A);
    double cov_max_final_max_AB_direct = covariance(max_final, max_AB);
    size_t cache2 = get_cov_matrix()->size();

    std::cout << "Cache size after full computation: " << cache1 << std::endl;
    std::cout << "Cache size after direct computation: " << cache2 << std::endl;

    // Find indices for comparison
    size_t idx_max_AB_max_CD = 14;  // {max_AB, max_CD}
    size_t idx_max_final_A = 15;    // {max_final, A}
    size_t idx_max_final_max_AB = 16;  // {max_final, max_AB}

    std::cout << "\nCov(max_AB, max_CD):" << std::endl;
    std::cout << "  Order 1 (full first): " << results1[idx_max_AB_max_CD] << std::endl;
    std::cout << "  Order 2 (direct): " << cov_max_AB_max_CD_direct << std::endl;
    std::cout << "  Difference: " << std::abs(results1[idx_max_AB_max_CD] - cov_max_AB_max_CD_direct) << std::endl;

    std::cout << "\nCov(max_final, A):" << std::endl;
    std::cout << "  Order 1 (full first): " << results1[idx_max_final_A] << std::endl;
    std::cout << "  Order 2 (direct): " << cov_max_final_A_direct << std::endl;
    std::cout << "  Difference: " << std::abs(results1[idx_max_final_A] - cov_max_final_A_direct) << std::endl;

    std::cout << "\nCov(max_final, max_AB):" << std::endl;
    std::cout << "  Order 1 (full first): " << results1[idx_max_final_max_AB] << std::endl;
    std::cout << "  Order 2 (direct): " << cov_max_final_max_AB_direct << std::endl;
    std::cout << "  Difference: " << std::abs(results1[idx_max_final_max_AB] - cov_max_final_max_AB_direct) << std::endl;

    // Allow small tolerance for numerical differences
    double tolerance = 1e-6;
    EXPECT_NEAR(results1[idx_max_AB_max_CD], cov_max_AB_max_CD_direct, tolerance);
    EXPECT_NEAR(results1[idx_max_final_A], cov_max_final_A_direct, tolerance);
    EXPECT_NEAR(results1[idx_max_final_max_AB], cov_max_final_max_AB_direct, tolerance);
}

// Test: Complex nested MAX with different computation orders
TEST_F(OrderDependencyTest, NestedMaxOrderIndependence) {
    std::cout << "\n=== Testing nested MAX order independence ===" << std::endl;
    std::cout << std::setprecision(12);

    // Create 5 Normal variables
    Normal N1(100.0, 100.0);
    Normal N2(110.0, 121.0);
    Normal N3(90.0, 81.0);
    Normal N4(105.0, 110.25);
    Normal N5(95.0, 90.25);

    // Build a tree of MAX operations
    RandomVar M12 = MAX(N1, N2);
    RandomVar M34 = MAX(N3, N4);
    RandomVar M1234 = MAX(M12, M34);
    RandomVar M_final = MAX(M1234, N5);

    // Order 1: Compute from leaves to root
    get_cov_matrix()->clear();
    covariance(N1, N2);
    covariance(N3, N4);
    covariance(M12, N3);
    covariance(M12, N4);
    covariance(M34, N1);
    covariance(M34, N2);
    covariance(M12, M34);
    covariance(M1234, N5);
    double cov1_final_N1 = covariance(M_final, N1);
    double cov1_final_M12 = covariance(M_final, M12);
    size_t cache1 = get_cov_matrix()->size();

    // Order 2: Compute directly
    get_cov_matrix()->clear();
    double cov2_final_N1 = covariance(M_final, N1);
    double cov2_final_M12 = covariance(M_final, M12);
    size_t cache2 = get_cov_matrix()->size();

    std::cout << "Cache size (order 1): " << cache1 << std::endl;
    std::cout << "Cache size (order 2): " << cache2 << std::endl;

    std::cout << "\nCov(M_final, N1):" << std::endl;
    std::cout << "  Order 1: " << cov1_final_N1 << std::endl;
    std::cout << "  Order 2: " << cov2_final_N1 << std::endl;
    std::cout << "  Difference: " << std::abs(cov1_final_N1 - cov2_final_N1) << std::endl;

    std::cout << "\nCov(M_final, M12):" << std::endl;
    std::cout << "  Order 1: " << cov1_final_M12 << std::endl;
    std::cout << "  Order 2: " << cov2_final_M12 << std::endl;
    std::cout << "  Difference: " << std::abs(cov1_final_M12 - cov2_final_M12) << std::endl;

    double tolerance = 1e-6;
    EXPECT_NEAR(cov1_final_N1, cov2_final_N1, tolerance);
    EXPECT_NEAR(cov1_final_M12, cov2_final_M12, tolerance);
}

// Test: Variance should be consistent regardless of covariance computation order
TEST_F(OrderDependencyTest, VarianceConsistency) {
    std::cout << "\n=== Testing variance consistency ===" << std::endl;
    std::cout << std::setprecision(12);

    Normal A(100.0, 100.0);
    Normal B(80.0, 64.0);
    Normal C(120.0, 144.0);

    RandomVar max_AB = MAX(A, B);
    RandomVar max_ABC = MAX(max_AB, C);

    // Get variance before any covariance computation
    get_cov_matrix()->clear();
    double var1 = max_ABC->variance();

    // Compute some covariances
    covariance(A, B);
    covariance(max_AB, C);
    covariance(A, max_ABC);

    // Get variance again
    double var2 = max_ABC->variance();

    // Clear and compute variance directly
    get_cov_matrix()->clear();
    double var3 = max_ABC->variance();

    std::cout << "Variance (before covariance): " << var1 << std::endl;
    std::cout << "Variance (after covariance): " << var2 << std::endl;
    std::cout << "Variance (fresh cache): " << var3 << std::endl;

    EXPECT_DOUBLE_EQ(var1, var2);
    EXPECT_DOUBLE_EQ(var1, var3);
}

