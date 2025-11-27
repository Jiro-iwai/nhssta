#include <gtest/gtest.h>

#include <cmath>
#include <limits>
#include <nhssta/exception.hpp>

#include "../src/add.hpp"
#include "../src/covariance.hpp"
#include "../src/max.hpp"
#include "../src/normal.hpp"
#include "../src/sub.hpp"
#include "../src/util_numerical.hpp"

using RandomVar = RandomVariable::RandomVariable;
using Normal = RandomVariable::Normal;

class CovarianceEdgeTest : public ::testing::Test {};

// Test: Very small input (G40 << G244 pattern)
TEST_F(CovarianceEdgeTest, VerySmallInput) {
    Normal large(100.0, 100.0);   // mean=100, std=10
    Normal small(10.0, 1.0);      // mean=10, std=1 (much smaller)
    
    RandomVar max_ls = MAX(large, small);
    
    // max(Large, Small) should be dominated by Large
    double mean_max = max_ls->mean();
    EXPECT_GT(mean_max, 90.0);
    
    // Test covariance with another variable
    Normal other(50.0, 25.0);
    RandomVar max_other = MAX(other, large);
    
    double cov = covariance(max_ls, max_other);
    
    // Both share 'large', so should have positive covariance
    // But if bug exists, might get negative
    std::cout << "Cov(MAX(large,small), MAX(other,large)) = " << cov << std::endl;
}

// Test: High correlation (rho ≈ 1)
TEST_F(CovarianceEdgeTest, HighCorrelation) {
    Normal base(100.0, 100.0);
    // Create highly correlated variables: d1 ≈ base + small_noise
    RandomVar d1 = base + Normal(0.0, 1.0);  // Very small noise
    RandomVar d2 = base + Normal(0.0, 1.0);
    
    RandomVar base_var = base;
    RandomVar max0_d1 = MAX0(d1 - base_var);  // max(0, small_noise)
    RandomVar max0_d2 = MAX0(d2 - base_var);
    
    double cov = covariance(max0_d1, max0_d2);
    
    // For highly correlated inputs, covariance should be positive
    std::cout << "Cov(max0(high_corr), max0(high_corr)) = " << cov << std::endl;
}

// Test: Negative correlation (rho ≈ -1)
TEST_F(CovarianceEdgeTest, NegativeCorrelation) {
    Normal base(100.0, 100.0);
    // Create negatively correlated: d1 = base, d2 = offset - base
    RandomVar d1 = base;
    Normal offset(200.0, 100.0);
    RandomVar d2 = offset - base;  // Negatively correlated
    
    Normal threshold(50.0, 1.0);
    RandomVar threshold_var = threshold;
    RandomVar max0_d1 = MAX0(d1 - threshold_var);
    RandomVar max0_d2 = MAX0(d2 - threshold_var);
    
    double cov = covariance(max0_d1, max0_d2);
    
    std::cout << "Cov(max0(neg_corr), max0(neg_corr)) = " << cov << std::endl;
    // For negatively correlated inputs, covariance can be negative
}

// Test: Independent case (rho = 0) - should have E[D0+D1+] = E[D0+] * E[D1+]
TEST_F(CovarianceEdgeTest, IndependentCase) {
    Normal d0(5.983693, 12.223010 * 12.223010);
    Normal d1(0.0, 2.006540 * 2.006540);
    
    RandomVar max0_d0 = MAX0(d0);
    RandomVar max0_d1 = MAX0(d1);
    
    double cov = covariance(max0_d0, max0_d1);
    
    double E0pos = RandomVariable::expected_positive_part(5.983693, 12.223010);
    double E1pos = RandomVariable::expected_positive_part(0.0, 2.006540);
    double expected_Eprod = E0pos * E1pos;
    
    double Eprod = RandomVariable::expected_prod_pos(5.983693, 12.223010, 0.0, 2.006540, 0.0);
    
    std::cout << "Independent case (rho=0):" << std::endl;
    std::cout << "  E[D0+] = " << E0pos << std::endl;
    std::cout << "  E[D1+] = " << E1pos << std::endl;
    std::cout << "  E[D0+] * E[D1+] = " << expected_Eprod << std::endl;
    std::cout << "  E[D0+D1+] (from expected_prod_pos) = " << Eprod << std::endl;
    std::cout << "  Difference: " << (expected_Eprod - Eprod) << std::endl;
    std::cout << "  Cov = " << cov << std::endl;
    
    // For independent case, E[D0+D1+] should equal E[D0+] * E[D1+]
    // But we're seeing E[D0+D1+] < E[D0+] * E[D1+], which causes negative covariance
    EXPECT_NEAR(Eprod, expected_Eprod, 0.01) << "For independent case, E[D0+D1+] should equal E[D0+] * E[D1+]";
}

// Test: MeanPhiMax with extreme values
TEST_F(CovarianceEdgeTest, MeanPhiMaxExtremeValues) {
    // Test MeanPhiMax with very negative ms (should return 1.0)
    double ms_very_neg = -10.0;
    double mpx_neg = RandomVariable::MeanPhiMax(ms_very_neg);
    EXPECT_NEAR(mpx_neg, 1.0, 0.01) << "MeanPhiMax(-10) should be ≈ 1.0";
    
    // Test MeanPhiMax with very positive ms (should return 0.0)
    double ms_very_pos = 10.0;
    double mpx_pos = RandomVariable::MeanPhiMax(ms_very_pos);
    EXPECT_NEAR(mpx_pos, 0.0, 0.01) << "MeanPhiMax(10) should be ≈ 0.0";
    
    // Test MeanPhiMax with zero (should be around 0.5)
    double ms_zero = 0.0;
    double mpx_zero = RandomVariable::MeanPhiMax(ms_zero);
    EXPECT_GT(mpx_zero, 0.4);
    EXPECT_LT(mpx_zero, 0.6);
}

// Test: expected_prod_pos with rho = 0 (independent case)
TEST_F(CovarianceEdgeTest, ExpectedProdPosIndependent) {
    double mu0 = 5.983693;
    double sigma0 = 12.223010;
    double mu1 = 0.0;
    double sigma1 = 2.006540;
    double rho = 0.0;
    
    double E0pos = RandomVariable::expected_positive_part(mu0, sigma0);
    double E1pos = RandomVariable::expected_positive_part(mu1, sigma1);
    double expected_Eprod = E0pos * E1pos;
    
    double Eprod = RandomVariable::expected_prod_pos(mu0, sigma0, mu1, sigma1, rho);
    
    std::cout << "expected_prod_pos with rho=0:" << std::endl;
    std::cout << "  E[D0+] * E[D1+] = " << expected_Eprod << std::endl;
    std::cout << "  E[D0+D1+] = " << Eprod << std::endl;
    std::cout << "  Difference: " << (expected_Eprod - Eprod) << std::endl;
    
    // For independent case, these should be equal
    // But we're seeing Eprod < expected_Eprod, which is the bug
    EXPECT_NEAR(Eprod, expected_Eprod, 0.01) << "For rho=0, E[D0+D1+] should equal E[D0+] * E[D1+]";
}

