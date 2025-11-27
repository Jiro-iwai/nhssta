/**
 * @file test_small_rho.cpp
 * @brief Investigation of small rho behavior for Issue #157
 *
 * Tests whether small rho values (|rho| ≈ 0) cause precision issues
 * in covariance calculations.
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

// Forward declarations from util_numerical.cpp
namespace RandomVariable {
double expected_positive_part(double mu, double sigma);
double expected_prod_pos(double mu0, double sigma0, double mu1, double sigma1, double rho);
}  // namespace RandomVariable

namespace RV = ::RandomVariable;
using RandomVar = RV::RandomVariable;
using RV::Normal;
using RV::MAX;
using RV::covariance;

static RV::CovarianceMatrix& get_cov_matrix() {
    return RV::get_covariance_matrix();
}

class SmallRhoTest : public ::testing::Test {
   protected:
    void SetUp() override {
        get_cov_matrix()->clear();
    }
};

// Test: expected_prod_pos behavior for small rho values
TEST_F(SmallRhoTest, ExpectedProdPosSmallRho) {
    std::cout << "\n=== Testing expected_prod_pos for small rho ===" << std::endl;
    std::cout << std::setprecision(12);

    // Typical gate delay parameters
    double mu0 = 100.0, sigma0 = 10.0;
    double mu1 = 80.0, sigma1 = 8.0;

    double E0 = RV::expected_positive_part(mu0, sigma0);
    double E1 = RV::expected_positive_part(mu1, sigma1);
    double E_independent = E0 * E1;

    std::cout << "E[D0⁺] = " << E0 << std::endl;
    std::cout << "E[D1⁺] = " << E1 << std::endl;
    std::cout << "E[D0⁺]*E[D1⁺] (independent) = " << E_independent << std::endl;

    std::vector<double> rho_values = {
        0.0, 1e-10, 1e-8, 1e-6, 1e-5, 1e-4, 1e-3, 0.001467, 0.01, 0.1,
        -1e-10, -1e-8, -1e-6, -1e-5, -1e-4, -1e-3, -0.001467, -0.01, -0.1
    };

    std::cout << "\nrho\t\t\tE[D0⁺D1⁺]\t\tCov(D0⁺,D1⁺)\t\tRelErr" << std::endl;
    std::cout << std::string(80, '-') << std::endl;

    for (double rho : rho_values) {
        double E_prod = RV::expected_prod_pos(mu0, sigma0, mu1, sigma1, rho);
        double cov = E_prod - E_independent;
        double rel_err = std::abs(cov) / E_independent;

        std::cout << std::setw(12) << rho << "\t"
                  << std::setw(16) << E_prod << "\t"
                  << std::setw(16) << cov << "\t"
                  << std::setw(12) << rel_err << std::endl;

        // For small |rho|, covariance should be very close to 0
        if (std::abs(rho) < 1e-4) {
            // Note: We're not asserting here, just observing
            if (std::abs(cov) > 1e-6) {
                std::cout << "  ** Note: |Cov| > 1e-6 for |rho| < 1e-4 **" << std::endl;
            }
        }
    }
}

// Test: Covariance between MAX operations with small correlations
TEST_F(SmallRhoTest, MaxCovarianceSmallRho) {
    std::cout << "\n=== Testing MAX covariance with small underlying correlations ===" << std::endl;
    std::cout << std::setprecision(12);

    // Create independent Normal variables (covariance = 0)
    Normal A(100.0, 100.0);
    Normal B(80.0, 64.0);
    Normal C(90.0, 81.0);
    Normal D(110.0, 121.0);

    // MAX operations
    RandomVar max_AB = MAX(A, B);
    RandomVar max_CD = MAX(C, D);

    // Covariance between two MAX operations with independent inputs
    get_cov_matrix()->clear();
    double cov = covariance(max_AB, max_CD);

    std::cout << "Cov(MAX(A,B), MAX(C,D)) = " << cov << std::endl;
    std::cout << "  (A, B, C, D are all independent)" << std::endl;

    // For independent inputs, the covariance should be very close to 0
    // (not exactly 0 due to numerical approximations in Clark method)
    std::cout << "  Expected: ~0 (numerical approximation)" << std::endl;

    // Check if the covariance is reasonably small
    double max_var = std::max(max_AB->variance(), max_CD->variance());
    double relative_cov = std::abs(cov) / max_var;
    std::cout << "  Relative to variance: " << relative_cov << std::endl;

    // The covariance should be very small relative to the variance
    EXPECT_LT(relative_cov, 0.01) << "Covariance should be small relative to variance";
}

// Test: Issue #157 scenario - rho = -0.001467
TEST_F(SmallRhoTest, Issue157Scenario) {
    std::cout << "\n=== Testing Issue #157 scenario: rho = -0.001467 ===" << std::endl;
    std::cout << std::setprecision(12);

    double mu0 = 100.0, sigma0 = 10.0;
    double mu1 = 100.0, sigma1 = 10.0;
    double rho = -0.001467;

    double E0 = RV::expected_positive_part(mu0, sigma0);
    double E1 = RV::expected_positive_part(mu1, sigma1);
    double E_independent = E0 * E1;

    double E_prod = RV::expected_prod_pos(mu0, sigma0, mu1, sigma1, rho);
    double cov = E_prod - E_independent;

    std::cout << "Parameters: mu0=" << mu0 << ", sigma0=" << sigma0 
              << ", mu1=" << mu1 << ", sigma1=" << sigma1 << ", rho=" << rho << std::endl;
    std::cout << "E[D0⁺] = " << E0 << std::endl;
    std::cout << "E[D1⁺] = " << E1 << std::endl;
    std::cout << "E[D0⁺]*E[D1⁺] = " << E_independent << std::endl;
    std::cout << "E[D0⁺ D1⁺] (GH) = " << E_prod << std::endl;
    std::cout << "Cov(D0⁺, D1⁺) = " << cov << std::endl;

    // For negative rho, we expect slightly negative covariance
    // The question is: is the magnitude reasonable?
    double expected_cov_approx = rho * sigma0 * sigma1;  // Rough approximation for Normal
    std::cout << "Approximate expected Cov (for Normal): " << expected_cov_approx << std::endl;

    // For small |rho|, the covariance should be small but can be slightly negative
    // This is mathematically correct behavior
    std::cout << "\nConclusion: Small negative covariance for negative rho is expected." << std::endl;
}

// Test: Verify that covariance sign matches rho sign for typical gate delays
TEST_F(SmallRhoTest, CovarianceSignMatchesRho) {
    std::cout << "\n=== Testing covariance sign matches rho sign ===" << std::endl;
    std::cout << std::setprecision(12);

    double mu0 = 100.0, sigma0 = 10.0;
    double mu1 = 100.0, sigma1 = 10.0;

    double E0 = RV::expected_positive_part(mu0, sigma0);
    double E1 = RV::expected_positive_part(mu1, sigma1);
    double E_independent = E0 * E1;

    std::vector<double> test_rhos = {0.1, 0.01, 0.001, -0.001, -0.01, -0.1};

    std::cout << "rho\t\t\tCov\t\t\tSign match?" << std::endl;
    std::cout << std::string(60, '-') << std::endl;

    for (double rho : test_rhos) {
        double E_prod = RV::expected_prod_pos(mu0, sigma0, mu1, sigma1, rho);
        double cov = E_prod - E_independent;
        bool sign_match = (rho > 0 && cov > 0) || (rho < 0 && cov < 0) || (rho == 0 && std::abs(cov) < 1e-10);

        std::cout << std::setw(12) << rho << "\t"
                  << std::setw(16) << cov << "\t"
                  << (sign_match ? "YES" : "NO") << std::endl;

        // For typical gate delays, sign should match
        EXPECT_TRUE(sign_match) << "Sign mismatch for rho=" << rho;
    }
}

