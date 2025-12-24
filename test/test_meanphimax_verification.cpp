/**
 * @file test_meanphimax_verification.cpp
 * @brief Verify MeanPhiMax implementation using Monte Carlo simulation
 *
 * This test verifies that the formula Cov(X, max(0,Z)) = Cov(X,Z) × Φ(μ_Z/σ_Z)
 * is correctly implemented in the numerical version.
 *
 * According to answer_1.md, if MeanPhiMax(x) = Φ(x), then using -μ/σ is wrong.
 * If MeanPhiMax(x) = Φ(-x) or 1-Φ(x), then -μ/σ is correct.
 */

#include <gtest/gtest.h>

#include <cmath>
#include <iomanip>
#include <iostream>
#include <random>

#include "covariance.hpp"
#include "max.hpp"
#include "normal.hpp"

namespace {

// Monte Carlo simulation to estimate Cov(X, max(0,Z))
// X and Z are jointly Gaussian with given parameters
struct JointGaussianParams {
    double mu_x, sigma_x;
    double mu_z, sigma_z;
    double rho;  // correlation coefficient
};

double monte_carlo_cov_x_max0z(const JointGaussianParams& params, int n_samples = 1000000) {
    std::mt19937_64 gen(12345);  // Fixed seed for reproducibility
    std::normal_distribution<double> dist(0.0, 1.0);

    double sum_x = 0.0;
    double sum_max0z = 0.0;
    double sum_x_max0z = 0.0;

    for (int i = 0; i < n_samples; ++i) {
        // Generate correlated standard normals
        double u1 = dist(gen);
        double u2 = dist(gen);

        // Transform to correlated (X, Z)
        double x = params.mu_x + params.sigma_x * u1;
        double z = params.mu_z + params.sigma_z * (params.rho * u1 + std::sqrt(1 - params.rho * params.rho) * u2);
        double max0z = std::max(0.0, z);

        sum_x += x;
        sum_max0z += max0z;
        sum_x_max0z += x * max0z;
    }

    double mean_x = sum_x / n_samples;
    double mean_max0z = sum_max0z / n_samples;
    double mean_x_max0z = sum_x_max0z / n_samples;

    return mean_x_max0z - mean_x * mean_max0z;
}

}  // namespace

class MeanPhiMaxVerificationTest : public ::testing::Test {
protected:
    void SetUp() override {
        RandomVariable::get_covariance_matrix()->clear();
    }

    void TearDown() override {
        RandomVariable::get_covariance_matrix()->clear();
    }
};

TEST_F(MeanPhiMaxVerificationTest, IndependentVariables_PositiveMean) {
    // Test case 1: X and Z independent, Z has positive mean
    // Cov(X,Z) = 0, so Cov(X, max(0,Z)) should be ≈ 0

    RandomVariable::Normal X(5.0, 4.0);   // μ=5, σ²=4
    RandomVariable::Normal Z(2.0, 9.0);   // μ=2, σ²=9
    RandomVariable::RandomVariable max0_Z = RandomVariable::MAX0(Z);

    double impl_cov = RandomVariable::covariance(X, max0_Z);

    JointGaussianParams params{5.0, 2.0, 2.0, 3.0, 0.0};  // rho=0 (independent)
    double mc_cov = monte_carlo_cov_x_max0z(params, 1000000);

    std::cout << "\n=== Test 1: Independent variables, Z has positive mean ===" << std::endl;
    std::cout << "  X ~ N(5, 4), Z ~ N(2, 9), Cov(X,Z) = 0" << std::endl;
    std::cout << "  Implementation: Cov(X, MAX0(Z)) = " << impl_cov << std::endl;
    std::cout << "  Monte Carlo:    Cov(X, MAX0(Z)) = " << mc_cov << std::endl;
    std::cout << "  Difference: " << std::abs(impl_cov - mc_cov) << std::endl;

    EXPECT_NEAR(impl_cov, mc_cov, 0.01) << "Independent case should match Monte Carlo";
    EXPECT_NEAR(impl_cov, 0.0, 0.01) << "Independent variables should have ~0 covariance";
}

TEST_F(MeanPhiMaxVerificationTest, PositiveCorrelation_PositiveMean) {
    // Test case 2: X and Z positively correlated, both positive mean
    // This is the critical test for MeanPhiMax sign

    RandomVariable::Normal X(10.0, 4.0);   // μ=10, σ²=4, σ=2
    RandomVariable::Normal Z(5.0, 9.0);    // μ=5, σ²=9, σ=3

    // Set correlation: Cov(X,Z) = ρ × σ_X × σ_Z = 0.6 × 2 × 3 = 3.6
    double rho = 0.6;
    double cov_xz = rho * 2.0 * 3.0;  // = 3.6
    RandomVariable::get_covariance_matrix()->set(X, Z, cov_xz);

    RandomVariable::RandomVariable max0_Z = RandomVariable::MAX0(Z);
    double impl_cov = RandomVariable::covariance(X, max0_Z);

    JointGaussianParams params{10.0, 2.0, 5.0, 3.0, rho};
    double mc_cov = monte_carlo_cov_x_max0z(params, 1000000);

    // Analytical: Cov(X, max(0,Z)) = Cov(X,Z) × Φ(μ_Z/σ_Z)
    double phi_factor = 0.5 * (1.0 + std::erf((5.0/3.0) / std::sqrt(2.0)));  // Φ(5/3)
    double analytical_cov = cov_xz * phi_factor;

    std::cout << "\n=== Test 2: Positive correlation, positive means ===" << std::endl;
    std::cout << "  X ~ N(10, 4), Z ~ N(5, 9), Cov(X,Z) = " << cov_xz << std::endl;
    std::cout << "  μ_Z/σ_Z = " << (5.0/3.0) << ", Φ(μ_Z/σ_Z) = " << phi_factor << std::endl;
    std::cout << "  Analytical:     Cov(X, MAX0(Z)) = " << analytical_cov << std::endl;
    std::cout << "  Implementation: Cov(X, MAX0(Z)) = " << impl_cov << std::endl;
    std::cout << "  Monte Carlo:    Cov(X, MAX0(Z)) = " << mc_cov << std::endl;
    std::cout << "  Impl vs MC diff: " << std::abs(impl_cov - mc_cov) << std::endl;
    std::cout << "  Impl vs Analytical diff: " << std::abs(impl_cov - analytical_cov) << std::endl;

    EXPECT_NEAR(impl_cov, mc_cov, 0.02) << "Implementation should match Monte Carlo";
    EXPECT_NEAR(impl_cov, analytical_cov, 0.02) << "Implementation should match analytical formula";
}

TEST_F(MeanPhiMaxVerificationTest, NegativeCorrelation_PositiveMean) {
    // Test case 3: X and Z negatively correlated

    RandomVariable::Normal X(8.0, 4.0);    // μ=8, σ²=4, σ=2
    RandomVariable::Normal Z(3.0, 9.0);    // μ=3, σ²=9, σ=3

    // Set negative correlation: Cov(X,Z) = -0.5 × 2 × 3 = -3.0
    double rho = -0.5;
    double cov_xz = rho * 2.0 * 3.0;  // = -3.0
    RandomVariable::get_covariance_matrix()->set(X, Z, cov_xz);

    RandomVariable::RandomVariable max0_Z = RandomVariable::MAX0(Z);
    double impl_cov = RandomVariable::covariance(X, max0_Z);

    JointGaussianParams params{8.0, 2.0, 3.0, 3.0, rho};
    double mc_cov = monte_carlo_cov_x_max0z(params, 1000000);

    // Analytical
    double phi_factor = 0.5 * (1.0 + std::erf((3.0/3.0) / std::sqrt(2.0)));  // Φ(1)
    double analytical_cov = cov_xz * phi_factor;

    std::cout << "\n=== Test 3: Negative correlation ===" << std::endl;
    std::cout << "  X ~ N(8, 4), Z ~ N(3, 9), Cov(X,Z) = " << cov_xz << std::endl;
    std::cout << "  μ_Z/σ_Z = 1.0, Φ(μ_Z/σ_Z) = " << phi_factor << std::endl;
    std::cout << "  Analytical:     Cov(X, MAX0(Z)) = " << analytical_cov << std::endl;
    std::cout << "  Implementation: Cov(X, MAX0(Z)) = " << impl_cov << std::endl;
    std::cout << "  Monte Carlo:    Cov(X, MAX0(Z)) = " << mc_cov << std::endl;
    std::cout << "  Impl vs MC diff: " << std::abs(impl_cov - mc_cov) << std::endl;

    EXPECT_NEAR(impl_cov, mc_cov, 0.02) << "Negative correlation case should match Monte Carlo";
    EXPECT_NEAR(impl_cov, analytical_cov, 0.02);
}

TEST_F(MeanPhiMaxVerificationTest, NegativeMean) {
    // Test case 4: Z has negative mean
    // When μ_Z < 0, max(0,Z) is mostly 0, so covariance should be small

    RandomVariable::Normal X(5.0, 4.0);    // μ=5, σ²=4, σ=2
    RandomVariable::Normal Z(-2.0, 9.0);   // μ=-2, σ²=9, σ=3

    double rho = 0.5;
    double cov_xz = rho * 2.0 * 3.0;  // = 3.0
    RandomVariable::get_covariance_matrix()->set(X, Z, cov_xz);

    RandomVariable::RandomVariable max0_Z = RandomVariable::MAX0(Z);
    double impl_cov = RandomVariable::covariance(X, max0_Z);

    JointGaussianParams params{5.0, 2.0, -2.0, 3.0, rho};
    double mc_cov = monte_carlo_cov_x_max0z(params, 1000000);

    // Analytical
    double phi_factor = 0.5 * (1.0 + std::erf((-2.0/3.0) / std::sqrt(2.0)));  // Φ(-2/3)
    double analytical_cov = cov_xz * phi_factor;

    std::cout << "\n=== Test 4: Z has negative mean ===" << std::endl;
    std::cout << "  X ~ N(5, 4), Z ~ N(-2, 9), Cov(X,Z) = " << cov_xz << std::endl;
    std::cout << "  μ_Z/σ_Z = " << (-2.0/3.0) << ", Φ(μ_Z/σ_Z) = " << phi_factor << std::endl;
    std::cout << "  Analytical:     Cov(X, MAX0(Z)) = " << analytical_cov << std::endl;
    std::cout << "  Implementation: Cov(X, MAX0(Z)) = " << impl_cov << std::endl;
    std::cout << "  Monte Carlo:    Cov(X, MAX0(Z)) = " << mc_cov << std::endl;
    std::cout << "  Impl vs MC diff: " << std::abs(impl_cov - mc_cov) << std::endl;

    EXPECT_NEAR(impl_cov, mc_cov, 0.02) << "Negative mean case should match Monte Carlo";
    EXPECT_NEAR(impl_cov, analytical_cov, 0.02);
}

TEST_F(MeanPhiMaxVerificationTest, ZeroMean) {
    // Test case 5: Z has zero mean
    // Φ(0) = 0.5, so Cov(X, max(0,Z)) = 0.5 × Cov(X,Z)

    RandomVariable::Normal X(5.0, 4.0);    // μ=5, σ²=4, σ=2
    RandomVariable::Normal Z(0.0, 9.0);    // μ=0, σ²=9, σ=3

    double rho = 0.7;
    double cov_xz = rho * 2.0 * 3.0;  // = 4.2
    RandomVariable::get_covariance_matrix()->set(X, Z, cov_xz);

    RandomVariable::RandomVariable max0_Z = RandomVariable::MAX0(Z);
    double impl_cov = RandomVariable::covariance(X, max0_Z);

    JointGaussianParams params{5.0, 2.0, 0.0, 3.0, rho};
    double mc_cov = monte_carlo_cov_x_max0z(params, 1000000);

    // Analytical: Φ(0) = 0.5
    double analytical_cov = cov_xz * 0.5;

    std::cout << "\n=== Test 5: Z has zero mean ===" << std::endl;
    std::cout << "  X ~ N(5, 4), Z ~ N(0, 9), Cov(X,Z) = " << cov_xz << std::endl;
    std::cout << "  μ_Z/σ_Z = 0, Φ(0) = 0.5" << std::endl;
    std::cout << "  Analytical:     Cov(X, MAX0(Z)) = " << analytical_cov << std::endl;
    std::cout << "  Implementation: Cov(X, MAX0(Z)) = " << impl_cov << std::endl;
    std::cout << "  Monte Carlo:    Cov(X, MAX0(Z)) = " << mc_cov << std::endl;
    std::cout << "  Impl vs MC diff: " << std::abs(impl_cov - mc_cov) << std::endl;

    EXPECT_NEAR(impl_cov, mc_cov, 0.02) << "Zero mean case should match Monte Carlo";
    EXPECT_NEAR(impl_cov, analytical_cov, 0.02);
    EXPECT_NEAR(impl_cov, cov_xz * 0.5, 0.02) << "Should be exactly half of Cov(X,Z)";
}
