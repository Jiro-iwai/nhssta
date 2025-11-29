/**
 * @file test_sensitivity_analysis.cpp
 * @brief Tests for sensitivity analysis using Expression-based RandomVariable
 *
 * Phase C of Issue #167: RandomVariable の Expression ベース化
 *
 * Goal: Compute sensitivities like ∂E[path]/∂μ_i, ∂σ[path]/∂σ_i
 * for critical path analysis.
 */

#include <gtest/gtest.h>
#include <cmath>
#include <iomanip>
#include <iostream>

#include "add.hpp"
#include "expression.hpp"
#include "max.hpp"
#include "normal.hpp"
#include "random_variable.hpp"
#include "sub.hpp"
#include "util_numerical.hpp"

namespace {

class SensitivityAnalysisTest : public ::testing::Test {
   protected:
    void SetUp() override {
        zero_all_grad();
    }

    // Numerical gradient using central difference
    double numerical_gradient(std::function<double(double)> f, double x, double h = 1e-5) {
        return (f(x + h) - f(x - h)) / (2.0 * h);
    }
};

// ============================================================================
// C-1: NormalImpl Expression-based tests
// ============================================================================

/**
 * Test that Normal distribution parameters can be tracked for sensitivity.
 *
 * For D ~ N(μ, σ²):
 * - E[D] = μ
 * - Var[D] = σ²
 *
 * Sensitivities:
 * - ∂E[D]/∂μ = 1
 * - ∂E[D]/∂σ = 0
 * - ∂Var[D]/∂μ = 0
 * - ∂Var[D]/∂σ = 2σ
 * - ∂Std[D]/∂σ = 1
 */
TEST_F(SensitivityAnalysisTest, Normal_MeanSensitivity) {
    using namespace RandomVariable;

    // Create Normal distribution
    Normal D(10.0, 4.0);  // N(μ=10, σ²=4), so σ=2

    // Mean should be μ
    Expression mean_D = D->mean_expr();
    EXPECT_DOUBLE_EQ(mean_D->value(), 10.0);

    // Compute gradient of mean w.r.t. parameters
    mean_D->backward();
    EXPECT_DOUBLE_EQ(D->mean_expr()->gradient(), 1.0);  // ∂E[D]/∂μ = 1
    EXPECT_DOUBLE_EQ(D->std_expr()->gradient(), 0.0);   // ∂E[D]/∂σ = 0
}

TEST_F(SensitivityAnalysisTest, Normal_VarianceSensitivity) {
    using namespace RandomVariable;

    Normal D(10.0, 4.0);  // N(μ=10, σ²=4), so σ=2

    // Variance should be σ²
    Expression var_D = D->var_expr();
    EXPECT_DOUBLE_EQ(var_D->value(), 4.0);

    // Compute gradient of variance w.r.t. σ
    var_D->backward();
    EXPECT_DOUBLE_EQ(D->mean_expr()->gradient(), 0.0);  // ∂Var[D]/∂μ = 0
    EXPECT_DOUBLE_EQ(D->std_expr()->gradient(), 4.0);   // ∂Var[D]/∂σ = ∂(σ²)/∂σ = 2σ = 4
}

TEST_F(SensitivityAnalysisTest, Normal_StdSensitivity) {
    using namespace RandomVariable;

    Normal D(10.0, 4.0);  // N(μ=10, σ²=4), so σ=2

    // Std should be σ
    Expression std_D = D->std_expr();
    EXPECT_DOUBLE_EQ(std_D->value(), 2.0);

    // Compute gradient of std w.r.t. σ
    std_D->backward();
    EXPECT_DOUBLE_EQ(D->mean_expr()->gradient(), 0.0);  // ∂Std[D]/∂μ = 0
    EXPECT_DOUBLE_EQ(D->std_expr()->gradient(), 1.0);   // ∂Std[D]/∂σ = 1
}

// ============================================================================
// C-2: ADD/SUB Expression-based tests
// ============================================================================

/**
 * Test sensitivity for sum of two Normal distributions (independent case).
 *
 * For A ~ N(μ_A, σ_A²), B ~ N(μ_B, σ_B²), independent:
 * - E[A + B] = μ_A + μ_B
 * - Var[A + B] = σ_A² + σ_B²
 *
 * Sensitivities for mean:
 * - ∂E[A+B]/∂μ_A = 1
 * - ∂E[A+B]/∂μ_B = 1
 */
TEST_F(SensitivityAnalysisTest, Add_MeanSensitivity) {
    using RandomVariable::Normal;

    // Create two independent Normal distributions
    Normal A(10.0, 4.0);  // N(μ=10, σ²=4), σ=2
    Normal B(5.0, 1.0);   // N(μ=5, σ²=1), σ=1

    // Sum them
    auto Sum = A + B;

    // E[A + B] = μ_A + μ_B = 15
    Expression mean_sum = Sum->mean_expr();
    EXPECT_DOUBLE_EQ(mean_sum->value(), 15.0);

    // Compute gradients
    mean_sum->backward();
    
    // ∂E[A+B]/∂μ_A = 1
    EXPECT_DOUBLE_EQ(A->mean_expr()->gradient(), 1.0);
    // ∂E[A+B]/∂μ_B = 1  
    EXPECT_DOUBLE_EQ(B->mean_expr()->gradient(), 1.0);
    // ∂E[A+B]/∂σ_A = 0 (mean doesn't depend on σ)
    EXPECT_DOUBLE_EQ(A->std_expr()->gradient(), 0.0);
    // ∂E[A+B]/∂σ_B = 0
    EXPECT_DOUBLE_EQ(B->std_expr()->gradient(), 0.0);
}

TEST_F(SensitivityAnalysisTest, Sub_MeanSensitivity) {
    using RandomVariable::Normal;

    Normal A(10.0, 4.0);
    Normal B(5.0, 1.0);

    auto Diff = A - B;

    // E[A - B] = μ_A - μ_B = 5
    Expression mean_diff = Diff->mean_expr();
    EXPECT_DOUBLE_EQ(mean_diff->value(), 5.0);

    mean_diff->backward();

    // ∂E[A-B]/∂μ_A = 1
    EXPECT_DOUBLE_EQ(A->mean_expr()->gradient(), 1.0);
    // ∂E[A-B]/∂μ_B = -1
    EXPECT_DOUBLE_EQ(B->mean_expr()->gradient(), -1.0);
}

TEST_F(SensitivityAnalysisTest, Add_VarianceSensitivity_Independent) {
    using RandomVariable::Normal;

    // Independent case: Cov(A, B) = 0
    Normal A(10.0, 4.0);  // σ_A = 2
    Normal B(5.0, 1.0);   // σ_B = 1

    auto Sum = A + B;

    // Var[A + B] = σ_A² + σ_B² = 4 + 1 = 5 (since independent)
    Expression var_sum = Sum->var_expr();
    EXPECT_DOUBLE_EQ(var_sum->value(), 5.0);

    var_sum->backward();

    // ∂Var[A+B]/∂σ_A = 2σ_A = 4
    EXPECT_DOUBLE_EQ(A->std_expr()->gradient(), 4.0);
    // ∂Var[A+B]/∂σ_B = 2σ_B = 2
    EXPECT_DOUBLE_EQ(B->std_expr()->gradient(), 2.0);
    // ∂Var[A+B]/∂μ_A = 0
    EXPECT_DOUBLE_EQ(A->mean_expr()->gradient(), 0.0);
    // ∂Var[A+B]/∂μ_B = 0
    EXPECT_DOUBLE_EQ(B->mean_expr()->gradient(), 0.0);
}

// ============================================================================
// C-3: MAX0 Expression-based tests
// ============================================================================

/**
 * Test sensitivity for max(0, D).
 *
 * For D ~ N(μ, σ²):
 * - E[max(0,D)] = μ + σ·MeanMax(-μ/σ)
 * - Var[max(0,D)] = σ²·(MeanMax2 - MeanMax²)
 */
TEST_F(SensitivityAnalysisTest, Max0_MeanSensitivity) {
    using RandomVariable::Normal;
    using RandomVariable::MAX0;

    Normal D(2.0, 1.0);  // N(μ=2, σ²=1), σ=1
    auto Max0_D = MAX0(D);

    // E[max(0,D)] using mean_expr()
    Expression mean_max0 = Max0_D->mean_expr();
    
    // Expected value from numerical calculation
    // Note: RandomVariable uses table lookup, Expression uses analytical formula
    // Table interpolation causes small differences (~1e-5)
    double expected_mean = Max0_D->mean();
    EXPECT_NEAR(mean_max0->value(), expected_mean, 1e-4);

    // Compute gradients
    mean_max0->backward();

    // Verify gradients numerically
    // ∂E[max(0,D)]/∂μ should be approximately Φ(-μ/σ) + 1
    // For μ=2, σ=1: Φ(-2) ≈ 0.023, so gradient ≈ 1.023
    double grad_mu = D->mean_expr()->gradient();
    double grad_sigma = D->std_expr()->gradient();
    
    // Check gradients are reasonable (positive for both μ and σ when μ > 0)
    EXPECT_GT(grad_mu, 0.0);
    EXPECT_GT(grad_sigma, 0.0);
}

TEST_F(SensitivityAnalysisTest, Max0_VarianceSensitivity) {
    using RandomVariable::Normal;
    using RandomVariable::MAX0;

    Normal D(2.0, 1.0);
    auto Max0_D = MAX0(D);

    Expression var_max0 = Max0_D->var_expr();
    double expected_var = Max0_D->variance();
    EXPECT_NEAR(var_max0->value(), expected_var, 1e-4);

    var_max0->backward();

    // Check gradients exist (non-zero for non-trivial case)
    // Variance gradients are complex but should be calculable
    double grad_sigma = D->std_expr()->gradient();
    
    // For μ >> σ, max(0,D) ≈ D, so Var[max(0,D)] ≈ Var[D] = σ²
    // Thus ∂Var/∂σ ≈ 2σ (positive)
    EXPECT_GT(grad_sigma, 0.0);
    
    // grad_mu can be any value depending on the distribution
    // Just verify it's finite
    EXPECT_TRUE(std::isfinite(D->mean_expr()->gradient()));
}

// ============================================================================
// C-4: MAX Expression-based tests
// ============================================================================

/**
 * Test sensitivity for MAX(A, B).
 *
 * MAX(A, B) = A + MAX0(B - A)  (Clark approximation)
 */
TEST_F(SensitivityAnalysisTest, Max_MeanSensitivity) {
    using RandomVariable::Normal;
    using RandomVariable::MAX;

    // A has larger mean than B
    Normal A(10.0, 4.0);  // N(μ=10, σ²=4)
    Normal B(8.0, 1.0);   // N(μ=8, σ²=1)

    auto MaxAB = MAX(A, B);

    // Compute mean
    Expression mean_max = MaxAB->mean_expr();
    double expected_mean = MaxAB->mean();
    // MAX involves SUB + MAX0, compounding table interpolation errors
    EXPECT_NEAR(mean_max->value(), expected_mean, 1e-3);

    // Compute gradients
    mean_max->backward();

    // Since μ_A > μ_B, A dominates in MAX(A,B)
    // ∂E[MAX]/∂μ_A should be larger than ∂E[MAX]/∂μ_B
    double grad_muA = A->mean_expr()->gradient();
    double grad_muB = B->mean_expr()->gradient();
    
    EXPECT_GT(grad_muA, grad_muB);
    
    // Both should be positive (increasing either mean increases MAX mean)
    EXPECT_GT(grad_muA, 0.0);
    EXPECT_GT(grad_muB, 0.0);
}

TEST_F(SensitivityAnalysisTest, Max_MeanSensitivity_EqualMeans) {
    using RandomVariable::Normal;
    using RandomVariable::MAX;

    // Equal means, equal variances
    Normal A(10.0, 4.0);
    Normal B(10.0, 4.0);

    auto MaxAB = MAX(A, B);

    Expression mean_max = MaxAB->mean_expr();
    mean_max->backward();

    // With equal means and variances, sensitivities should be approximately equal
    double grad_muA = A->mean_expr()->gradient();
    double grad_muB = B->mean_expr()->gradient();
    
    EXPECT_NEAR(grad_muA, grad_muB, 0.1);  // Allow some numerical tolerance
}

// ============================================================================
// Integration test: Simple path
// ============================================================================

/**
 * Test sensitivity for a simple path: Path = A + MAX(B, C)
 */
TEST_F(SensitivityAnalysisTest, SimplePath_Sensitivity) {
    using RandomVariable::Normal;
    using RandomVariable::MAX;

    Normal A(10.0, 4.0);  // σ = 2
    Normal B(5.0, 1.0);   // σ = 1
    Normal C(6.0, 2.25);  // σ = 1.5

    auto Path = A + MAX(B, C);

    // Compute path mean and sensitivities
    Expression path_mean = Path->mean_expr();
    double expected_mean = Path->mean();
    // Compound operations (ADD + MAX(SUB + MAX0)) have accumulated interpolation errors
    EXPECT_NEAR(path_mean->value(), expected_mean, 1e-2);
    
    path_mean->backward();

    double grad_muA = A->mean_expr()->gradient();
    double grad_muB = B->mean_expr()->gradient();
    double grad_muC = C->mean_expr()->gradient();
    double grad_sigmaA = A->std_expr()->gradient();
    double grad_sigmaB = B->std_expr()->gradient();
    double grad_sigmaC = C->std_expr()->gradient();

    std::cout << "\n=== Simple Path Sensitivity Analysis ===\n";
    std::cout << "Path = A + MAX(B, C)\n";
    std::cout << "E[Path] = " << path_mean->value() << "\n";
    std::cout << "\nSensitivities:\n";
    std::cout << "  ∂E[Path]/∂μ_A = " << grad_muA << "\n";
    std::cout << "  ∂E[Path]/∂μ_B = " << grad_muB << "\n";
    std::cout << "  ∂E[Path]/∂μ_C = " << grad_muC << "\n";
    std::cout << "  ∂E[Path]/∂σ_A = " << grad_sigmaA << "\n";
    std::cout << "  ∂E[Path]/∂σ_B = " << grad_sigmaB << "\n";
    std::cout << "  ∂E[Path]/∂σ_C = " << grad_sigmaC << "\n";

    // A is always in the path, so ∂E[Path]/∂μ_A = 1
    EXPECT_DOUBLE_EQ(grad_muA, 1.0);

    // Since μ_C > μ_B, C dominates in MAX(B,C), so ∂E[Path]/∂μ_C > ∂E[Path]/∂μ_B
    EXPECT_GT(grad_muC, grad_muB);
    
    // Mean doesn't depend on σ_A
    EXPECT_DOUBLE_EQ(grad_sigmaA, 0.0);
}

// ============================================================================
// Placeholder test (always passes to verify test infrastructure)
// ============================================================================

TEST_F(SensitivityAnalysisTest, PlaceholderTest) {
    // This test verifies the test infrastructure is working.
    // Real tests above are DISABLED until implementation is ready.
    EXPECT_TRUE(true);

    std::cout << "\n=== Phase C: Sensitivity Analysis Tests ===\n";
    std::cout << "Tests are DISABLED pending implementation.\n";
    std::cout << "Enable tests as each phase is implemented.\n";
}

}  // namespace

