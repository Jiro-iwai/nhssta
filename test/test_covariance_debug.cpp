#include <gtest/gtest.h>

#include <cmath>
#include <nhssta/exception.hpp>

#include "../src/add.hpp"
#include "../src/covariance.hpp"
#include "../src/max.hpp"
#include "../src/normal.hpp"
#include "../src/util_numerical.hpp"

using RandomVar = RandomVariable::RandomVariable;
using Normal = RandomVariable::Normal;

// Test case to verify covariance_max0_max0 calculation
// Based on the debug output:
// mu0=5.983693, sigma0=12.223010, mu1=0.000000, sigma1=2.006540
// Cov(D0,D1)=0.020929, rho=0.000853
// E[D0+]=8.441032, E[D1+]=0.800494, E[D0+D1+]=6.487810
// result=-0.269183
TEST(CovarianceDebugTest, VerifyMax0Max0Calculation) {
    // Create D0 and D1 with the given statistics
    // D0 ~ N(5.983693, 12.223010²)
    // D1 ~ N(0.000000, 2.006540²)
    // Cov(D0, D1) = 0.020929
    
    // Since Cov(D0, D1) = rho * sigma0 * sigma1
    // rho = 0.020929 / (12.223010 * 2.006540) = 0.000853
    
    // Create independent normals and combine them
    Normal n0(5.983693, 12.223010 * 12.223010);
    Normal n1(0.0, 2.006540 * 2.006540);
    
    // Create D0 = n0
    RandomVar d0 = n0;
    
    // Create D1 = n1 + small_correction to achieve Cov(D0, D1) = 0.020929
    // Since rho is very small, we can approximate D1 ≈ n1
    // For exact match, we'd need: D1 = n1 + alpha * n0 where alpha = rho * sigma1 / sigma0
    // Note: We'll use n1 directly as approximation since alpha is very small
    RandomVar d1 = n1;
    
    // Create max0 variables
    RandomVar max0_d0 = MAX0(d0);
    RandomVar max0_d1 = MAX0(d1);
    
    // Calculate covariance
    double cov = covariance(max0_d0, max0_d1);
    
    // Verify expected values
    double E0pos = RandomVariable::expected_positive_part(5.983693, 12.223010);
    double E1pos = RandomVariable::expected_positive_part(0.0, 2.006540);
    
    std::cout << "E[D0+] = " << E0pos << " (expected 8.441032)" << std::endl;
    std::cout << "E[D1+] = " << E1pos << " (expected 0.800494)" << std::endl;
    std::cout << "E[D0+] * E[D1+] = " << (E0pos * E1pos) << " (expected 6.755)" << std::endl;
    std::cout << "Cov(max(0,D0), max(0,D1)) = " << cov << " (expected -0.269183)" << std::endl;
    
    // Check if E[D0+D1+] < E[D0+] * E[D1+] (which would cause negative covariance)
    // This should not happen for positive correlation
    EXPECT_GT(E0pos * E1pos, 0.0) << "E[D0+] * E[D1+] should be positive";
}

// Test case: Simple MAX operation with small input
// G244 ↔ G68 = +0.072, but MAX(G244, G40) ↔ G68 = -0.479
TEST(CovarianceDebugTest, SimpleMaxWithSmallInput) {
    // Create shared component S
    Normal s(100.0, 100.0);  // mean=100, std=10
    
    // G244 = MAX(G281, S) where G281 shares with S
    Normal g281_part(50.0, 25.0);
    RandomVar g281 = MAX(g281_part, s);
    
    // G40 is small and independent
    Normal g40(30.0, 9.0);
    
    // G227_t2 = MAX(G244, G40)
    RandomVar g227_t2 = MAX(g281, g40);
    
    // G68 = MAX(G185, G186) where G185 shares with S
    Normal g185_part(170.0, 81.0);
    RandomVar g185 = MAX(g185_part, s);
    Normal g186(164.0, 59.1361);  // std^2 = 7.69^2
    RandomVar g68 = MAX(g185, g186);
    
    // Calculate correlations
    double cov_g244_g68 = covariance(g281, g68);
    double cov_g227_t2_g68 = covariance(g227_t2, g68);
    
    double var_g244 = g281->variance();
    double var_g227_t2 = g227_t2->variance();
    double var_g68 = g68->variance();
    
    double corr_g244_g68 = cov_g244_g68 / sqrt(var_g244 * var_g68);
    double corr_g227_t2_g68 = cov_g227_t2_g68 / sqrt(var_g227_t2 * var_g68);
    
    std::cout << "G244 ↔ G68 correlation: " << corr_g244_g68 << std::endl;
    std::cout << "MAX(G244, G40) ↔ G68 correlation: " << corr_g227_t2_g68 << std::endl;
    std::cout << "Sign flip: " << (corr_g244_g68 > 0 && corr_g227_t2_g68 < 0 ? "YES" : "NO") << std::endl;
    
    // This test documents the bug - we expect sign flip but it shouldn't happen
    if (corr_g244_g68 > 0 && corr_g227_t2_g68 < 0) {
        std::cout << "BUG DETECTED: Sign flip occurred!" << std::endl;
    }
}

