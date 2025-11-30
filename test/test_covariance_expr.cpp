/**
 * @file test_covariance_expr.cpp
 * @brief Tests for Expression-based covariance calculation (cov_expr)
 *
 * Phase C-5 of Issue #167: Expression-based covariance
 *
 * Test structure follows the implementation plan in docs/sensitivity_analysis_plan.md:
 * - C-5.1: Basic structure and cache
 * - C-5.2: Normal × Normal base cases
 * - C-5.3: ADD/SUB linear expansion
 * - C-5.4: MAX0 × X
 * - C-5.5: MAX0 × MAX0
 * - C-5.6: MAX expansion
 * - C-5.7: var_expr() integration
 */

#include <gtest/gtest.h>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <functional>

#include "expression.hpp"
#include "random_variable.hpp"
#include "normal.hpp"
#include "add.hpp"
#include "sub.hpp"
#include "max.hpp"
#include "covariance.hpp"

namespace RandomVariable {

class CovarianceExprTest : public ::testing::Test {
   protected:
    void SetUp() override {
        zero_all_grad();
        // Clear covariance caches before each test
        get_covariance_matrix()->clear();
        clear_cov_expr_cache();
    }

    // Numerical gradient using central difference
    double numerical_gradient(std::function<double(double)> f, double x, double h = 1e-5) {
        return (f(x + h) - f(x - h)) / (2.0 * h);
    }
};

// ============================================================================
// C-5.1: Basic structure and cache tests
// ============================================================================

TEST_F(CovarianceExprTest, CovExpr_SameVariable_ReturnsVariance) {
    // cov_expr(A, A) should return A->var_expr()
    Normal A(10.0, 4.0);  // μ=10, σ²=4

    Expression cov_aa = cov_expr(A, A);

    EXPECT_DOUBLE_EQ(cov_aa->value(), 4.0);  // Var(A) = σ² = 4
}

TEST_F(CovarianceExprTest, CovExpr_Symmetry) {
    // cov_expr(A, B) should equal cov_expr(B, A)
    Normal A(10.0, 4.0);
    Normal B(5.0, 1.0);
    auto Sum = A + B;

    Expression cov_ab = cov_expr(A, Sum);
    Expression cov_ba = cov_expr(Sum, A);

    EXPECT_DOUBLE_EQ(cov_ab->value(), cov_ba->value());
}

TEST_F(CovarianceExprTest, CovExpr_CacheHit) {
    // Second call should return cached Expression (same pointer)
    Normal A(10.0, 4.0);
    Normal B(5.0, 1.0);

    Expression cov1 = cov_expr(A, B);
    Expression cov2 = cov_expr(A, B);

    // Same Expression should be returned (pointer equality)
    EXPECT_EQ(cov1.get(), cov2.get());
}

TEST_F(CovarianceExprTest, CovExpr_CacheSymmetric) {
    // cov_expr(A, B) and cov_expr(B, A) should hit same cache entry
    Normal A(10.0, 4.0);
    Normal B(5.0, 1.0);

    Expression cov_ab = cov_expr(A, B);
    Expression cov_ba = cov_expr(B, A);

    // Same Expression should be returned
    EXPECT_EQ(cov_ab.get(), cov_ba.get());
}

// ============================================================================
// C-5.2: Normal × Normal base cases
// ============================================================================

TEST_F(CovarianceExprTest, CovExpr_Normal_SameInstance) {
    // Cov(N, N) = Var(N)
    Normal N(10.0, 4.0);

    Expression cov_nn = cov_expr(N, N);

    EXPECT_DOUBLE_EQ(cov_nn->value(), 4.0);

    // Gradient: ∂Var(N)/∂σ = 2σ
    cov_nn->backward();
    EXPECT_DOUBLE_EQ(N->std_expr()->gradient(), 4.0);  // 2 * 2 = 4
}

TEST_F(CovarianceExprTest, CovExpr_Normal_Independent) {
    // Cov(N1, N2) = 0 for independent normals
    Normal N1(10.0, 4.0);
    Normal N2(5.0, 1.0);

    Expression cov_12 = cov_expr(N1, N2);

    EXPECT_DOUBLE_EQ(cov_12->value(), 0.0);
}

TEST_F(CovarianceExprTest, CovExpr_Normal_IndependentGradient) {
    // Gradient of Cov(N1, N2) = 0 should be zero for all parameters
    Normal N1(10.0, 4.0);
    Normal N2(5.0, 1.0);

    Expression cov_12 = cov_expr(N1, N2);
    cov_12->backward();

    EXPECT_DOUBLE_EQ(N1->mean_expr()->gradient(), 0.0);
    EXPECT_DOUBLE_EQ(N1->std_expr()->gradient(), 0.0);
    EXPECT_DOUBLE_EQ(N2->mean_expr()->gradient(), 0.0);
    EXPECT_DOUBLE_EQ(N2->std_expr()->gradient(), 0.0);
}

// ============================================================================
// C-5.3: ADD/SUB linear expansion tests
// ============================================================================

TEST_F(CovarianceExprTest, CovExpr_Add_LinearExpansion) {
    // Cov(A+B, C) = Cov(A, C) + Cov(B, C)
    Normal A(10.0, 4.0);
    Normal B(5.0, 1.0);
    Normal C(8.0, 2.25);

    auto Sum = A + B;

    Expression cov_sum_c = cov_expr(Sum, C);
    Expression cov_a_c = cov_expr(A, C);
    Expression cov_b_c = cov_expr(B, C);

    // For independent normals, all should be 0
    EXPECT_DOUBLE_EQ(cov_sum_c->value(), 0.0);
    EXPECT_DOUBLE_EQ(cov_a_c->value() + cov_b_c->value(), 0.0);
}

TEST_F(CovarianceExprTest, CovExpr_Add_WithSelf) {
    // Cov(A+B, A) = Cov(A, A) + Cov(B, A) = Var(A) + 0 = Var(A)
    Normal A(10.0, 4.0);
    Normal B(5.0, 1.0);

    auto Sum = A + B;

    Expression cov_sum_a = cov_expr(Sum, A);

    EXPECT_DOUBLE_EQ(cov_sum_a->value(), 4.0);  // Var(A) = 4
}

TEST_F(CovarianceExprTest, CovExpr_Add_WithSelf_Gradient) {
    // Gradient of Cov(A+B, A) = Var(A) w.r.t. σ_A
    Normal A(10.0, 4.0);
    Normal B(5.0, 1.0);

    auto Sum = A + B;

    Expression cov_sum_a = cov_expr(Sum, A);
    cov_sum_a->backward();

    // ∂Cov(A+B, A)/∂σ_A = ∂Var(A)/∂σ_A = 2σ_A = 4
    EXPECT_DOUBLE_EQ(A->std_expr()->gradient(), 4.0);
}

TEST_F(CovarianceExprTest, CovExpr_Sub_LinearExpansion) {
    // Cov(A-B, C) = Cov(A, C) - Cov(B, C)
    Normal A(10.0, 4.0);
    Normal B(5.0, 1.0);
    Normal C(8.0, 2.25);

    auto Diff = A - B;

    Expression cov_diff_c = cov_expr(Diff, C);
    Expression cov_a_c = cov_expr(A, C);
    Expression cov_b_c = cov_expr(B, C);

    // For independent normals, all should be 0
    EXPECT_DOUBLE_EQ(cov_diff_c->value(), 0.0);
    EXPECT_DOUBLE_EQ(cov_a_c->value() - cov_b_c->value(), 0.0);
}

TEST_F(CovarianceExprTest, CovExpr_Sub_WithSelf) {
    // Cov(A-B, A) = Cov(A, A) - Cov(B, A) = Var(A) - 0 = Var(A)
    Normal A(10.0, 4.0);
    Normal B(5.0, 1.0);

    auto Diff = A - B;

    Expression cov_diff_a = cov_expr(Diff, A);

    EXPECT_DOUBLE_EQ(cov_diff_a->value(), 4.0);  // Var(A) = 4
}

TEST_F(CovarianceExprTest, CovExpr_Sub_WithNegatedSelf) {
    // Cov(A-B, B) = Cov(A, B) - Cov(B, B) = 0 - Var(B) = -Var(B)
    Normal A(10.0, 4.0);
    Normal B(5.0, 1.0);

    auto Diff = A - B;

    Expression cov_diff_b = cov_expr(Diff, B);

    EXPECT_DOUBLE_EQ(cov_diff_b->value(), -1.0);  // -Var(B) = -1
}

// ============================================================================
// C-5.4: MAX0 × X tests
// ============================================================================

TEST_F(CovarianceExprTest, CovExpr_Max0_WithArgument) {
    // Cov(X, MAX0(X)) should use cov_x_max0_expr
    Normal X(1.0, 1.0);  // μ=1, σ=1

    auto Max0_X = MAX0(X);

    Expression cov_x_max0 = cov_expr(X, Max0_X);

    // Compare with numerical covariance
    // Note: Expression uses analytical Phi, RandomVariable uses table-based MeanPhiMax
    // Actual error ~0.0006, tolerance set to 8e-4
    double numerical_cov = covariance(X, Max0_X);
    EXPECT_NEAR(cov_x_max0->value(), numerical_cov, 8e-4);
}

TEST_F(CovarianceExprTest, CovExpr_Max0_WithIndependent) {
    // Cov(Y, MAX0(X)) where Y and X are independent
    Normal X(1.0, 1.0);
    Normal Y(5.0, 4.0);

    auto Max0_X = MAX0(X);

    Expression cov_y_max0x = cov_expr(Y, Max0_X);

    // Independent → Cov = 0
    EXPECT_NEAR(cov_y_max0x->value(), 0.0, 1e-10);
}

TEST_F(CovarianceExprTest, CovExpr_Max0_Gradient) {
    // Gradient of Cov(X, MAX0(X)) w.r.t. μ_X and σ_X
    Normal X(2.0, 1.0);  // μ=2, σ=1

    auto Max0_X = MAX0(X);

    Expression cov_x_max0 = cov_expr(X, Max0_X);
    double cov_value = cov_x_max0->value();

    cov_x_max0->backward();

    // Numerical verification
    double grad_mu_numerical = numerical_gradient(
        [](double mu) {
            Normal X_test(mu, 1.0);
            auto Max0_test = MAX0(X_test);
            return covariance(X_test, Max0_test);
        },
        2.0);

    double grad_sigma_numerical = numerical_gradient(
        [](double sigma) {
            Normal X_test(2.0, sigma * sigma);  // variance = sigma^2
            auto Max0_test = MAX0(X_test);
            return covariance(X_test, Max0_test);
        },
        1.0);

    // Note: Numerical gradient uses table-based covariance, analytical gradient uses Phi
    // Larger tolerance accounts for method differences
    EXPECT_NEAR(X->mean_expr()->gradient(), grad_mu_numerical, 1e-2);
    EXPECT_NEAR(X->std_expr()->gradient(), grad_sigma_numerical, 2e-2);
}

// ============================================================================
// C-5.5: MAX0 × MAX0 tests
// ============================================================================

TEST_F(CovarianceExprTest, CovExpr_Max0_Max0_Same) {
    // Cov(MAX0(X), MAX0(X)) = Var(MAX0(X))
    Normal X(1.0, 1.0);

    auto Max0_X = MAX0(X);

    Expression cov_max0_max0 = cov_expr(Max0_X, Max0_X);

    // Should equal variance
    // Note: Expression uses analytical formulas, RandomVariable uses tables
    double expected_var = Max0_X->variance();
    EXPECT_NEAR(cov_max0_max0->value(), expected_var, 2e-3);
}

TEST_F(CovarianceExprTest, CovExpr_Max0_Max0_Different) {
    // Cov(MAX0(X), MAX0(Y)) for independent X, Y
    // When X and Y are independent, Cov(max0(X), max0(Y)) = 0 theoretically
    Normal X(1.0, 1.0);
    Normal Y(2.0, 1.0);

    auto Max0_X = MAX0(X);
    auto Max0_Y = MAX0(Y);

    Expression cov_max0x_max0y = cov_expr(Max0_X, Max0_Y);

    // Independent variables: covariance should be 0
    // Expression correctly computes 0 (analytical)
    // Note: RandomVariable::covariance uses Gauss-Hermite which may have small numerical errors
    EXPECT_NEAR(cov_max0x_max0y->value(), 0.0, 1e-10);
}

TEST_F(CovarianceExprTest, CovExpr_Max0_Max0_Correlated) {
    // Cov(MAX0(D1), MAX0(D2)) where D1 and D2 share variables
    // Use a case with |ρ| < 1 (not perfectly correlated)
    // Note: ρ = ±1 causes division by zero in analytical formula (bivariate normal degeneracy)
    Normal A(10.0, 4.0);
    Normal B(8.0, 1.0);
    Normal C(5.0, 0.25);  // Third variable to break perfect correlation

    auto D1 = A - B;      // ρ(D1, D2) will be partial, not ±1
    auto D2 = B - A + C;  // Not perfectly negatively correlated with D1

    auto Max0_D1 = MAX0(D1);
    auto Max0_D2 = MAX0(D2);

    Expression cov_expr_result = cov_expr(Max0_D1, Max0_D2);

    // Compare with numerical covariance
    // Note: Expression uses analytical Phi2, RandomVariable uses Gauss-Hermite
    // For highly correlated cases, the difference can be significant (~3%)
    double numerical_cov = covariance(Max0_D1, Max0_D2);
    EXPECT_NEAR(cov_expr_result->value(), numerical_cov, 0.15);
}

TEST_F(CovarianceExprTest, CovExpr_Max0_Max0_SameComputation) {
    // Edge case: Two different MAX0 objects with the same computation (ρ = 1)
    // This tests the ρ ≈ 1 handling in cov_max0_max0_expr_impl
    Normal A(10.0, 4.0);
    Normal B(8.0, 1.0);

    auto Sum1 = A + B;
    auto Sum2 = A + B;  // Different object, same computation
    auto Max0_Sum1 = MAX0(Sum1);
    auto Max0_Sum2 = MAX0(Sum2);

    // These are different objects but have ρ = 1
    EXPECT_NE(Max0_Sum1.get(), Max0_Sum2.get());  // Different objects

    Expression cov_expr_result = cov_expr(Max0_Sum1, Max0_Sum2);

    // Should not diverge! Should return Var(MAX0(Sum)) since ρ ≈ 1
    // ρ = 1 case uses same var_expr path, so exact match expected
    double expected_var = Max0_Sum1->variance();
    EXPECT_NEAR(cov_expr_result->value(), expected_var, 1e-10);
    EXPECT_FALSE(std::isnan(cov_expr_result->value()));
    EXPECT_FALSE(std::isinf(cov_expr_result->value()));
}

TEST_F(CovarianceExprTest, CovExpr_Max0_Max0_Gradient) {
    // Gradient of Cov(MAX0(D1), MAX0(D2)) w.r.t. input parameters
    Normal A(10.0, 4.0);  // μ_A=10, σ_A=2
    Normal B(8.0, 1.0);   // μ_B=8, σ_B=1

    auto D = A - B;
    auto Max0_D = MAX0(D);

    Expression cov_max0_max0 = cov_expr(Max0_D, Max0_D);
    cov_max0_max0->backward();

    // Variance of MAX0(D) depends on μ_A, σ_A, μ_B, σ_B
    // Gradients should be non-zero
    // Just verify they exist and are reasonable
    std::cout << "\n=== MAX0×MAX0 Gradients ===\n";
    std::cout << "∂Cov/∂μ_A = " << A->mean_expr()->gradient() << "\n";
    std::cout << "∂Cov/∂σ_A = " << A->std_expr()->gradient() << "\n";
    std::cout << "∂Cov/∂μ_B = " << B->mean_expr()->gradient() << "\n";
    std::cout << "∂Cov/∂σ_B = " << B->std_expr()->gradient() << "\n";

    // For Var(MAX0(A-B)), increasing μ_A should increase variance (more probability mass in positive region)
    // This is a sanity check, not an exact value test
    EXPECT_GT(A->mean_expr()->gradient(), 0.0);
}

// ============================================================================
// C-5.6: MAX expansion tests
// ============================================================================

TEST_F(CovarianceExprTest, CovExpr_Max_Expansion) {
    // Cov(MAX(A,B), C) = Cov(A + MAX0(B-A), C) = Cov(A, C) + Cov(MAX0(B-A), C)
    Normal A(10.0, 4.0);
    Normal B(8.0, 1.0);
    Normal C(5.0, 2.25);

    auto MaxAB = MAX(A, B);

    Expression cov_max_c = cov_expr(MaxAB, C);

    // Compare with numerical covariance
    double numerical_cov = covariance(MaxAB, C);
    EXPECT_NEAR(cov_max_c->value(), numerical_cov, 1e-5);
}

TEST_F(CovarianceExprTest, CovExpr_Max_WithSelf) {
    // Cov(MAX(A,B), A)
    Normal A(10.0, 4.0);
    Normal B(8.0, 1.0);

    auto MaxAB = MAX(A, B);

    Expression cov_max_a = cov_expr(MaxAB, A);

    // Compare with numerical covariance
    // Note: Expression uses analytical formulas, RandomVariable uses tables
    // Actual error ~0.0007, tolerance set to 8e-4
    double numerical_cov = covariance(MaxAB, A);
    EXPECT_NEAR(cov_max_a->value(), numerical_cov, 8e-4);
}

TEST_F(CovarianceExprTest, CovExpr_Max_Max_Same) {
    // Cov(MAX(A,B), MAX(A,B)) = Var(MAX(A,B))
    Normal A(10.0, 4.0);
    Normal B(8.0, 1.0);

    auto MaxAB = MAX(A, B);

    Expression cov_max_max = cov_expr(MaxAB, MaxAB);

    // Note: Both use var_expr() path, tolerance accounts for analytical vs table differences
    // Actual error ~0.0001, tolerance set to 2e-4
    double expected_var = MaxAB->variance();
    EXPECT_NEAR(cov_max_max->value(), expected_var, 2e-4);
}

TEST_F(CovarianceExprTest, CovExpr_Max_Max_Different) {
    // Cov(MAX(A,B), MAX(C,D)) where A,B,C,D are all independent
    Normal A(10.0, 4.0);
    Normal B(8.0, 1.0);
    Normal C(12.0, 2.25);
    Normal D(9.0, 1.0);

    auto MaxAB = MAX(A, B);
    auto MaxCD = MAX(C, D);

    Expression cov_max_max = cov_expr(MaxAB, MaxCD);

    // Independent MAXes should have zero covariance
    // Expression correctly computes 0 (analytical)
    // Note: RandomVariable::covariance uses Gauss-Hermite which may have small numerical errors
    EXPECT_NEAR(cov_max_max->value(), 0.0, 1e-10);
}

TEST_F(CovarianceExprTest, CovExpr_Max_Gradient) {
    // Gradient of Cov(MAX(A,B), A) w.r.t. input parameters
    Normal A(10.0, 4.0);
    Normal B(8.0, 1.0);

    auto MaxAB = MAX(A, B);

    Expression cov_max_a = cov_expr(MaxAB, A);
    cov_max_a->backward();

    std::cout << "\n=== MAX Covariance Gradients ===\n";
    std::cout << "Cov(MAX(A,B), A) = " << cov_max_a->value() << "\n";
    std::cout << "∂Cov/∂μ_A = " << A->mean_expr()->gradient() << "\n";
    std::cout << "∂Cov/∂σ_A = " << A->std_expr()->gradient() << "\n";
    std::cout << "∂Cov/∂μ_B = " << B->mean_expr()->gradient() << "\n";
    std::cout << "∂Cov/∂σ_B = " << B->std_expr()->gradient() << "\n";

    // Gradients should exist
    // Since A dominates (μ_A > μ_B), ∂Cov/∂σ_A should be positive
    EXPECT_GT(A->std_expr()->gradient(), 0.0);
}

// ============================================================================
// C-5.7: var_expr() integration tests
// ============================================================================

TEST_F(CovarianceExprTest, VarExpr_Add_WithSharedInput) {
    // Var(A + A) = Var(A) + 2*Cov(A,A) + Var(A) = 4*Var(A)
    Normal A(10.0, 4.0);

    auto Sum = A + A;

    Expression var_sum = Sum->var_expr();

    // Expected: 4 * Var(A) = 4 * 4 = 16
    EXPECT_NEAR(var_sum->value(), 16.0, 1e-10);
}

TEST_F(CovarianceExprTest, VarExpr_Add_WithSharedInput_Gradient) {
    // Gradient of Var(A + A) = 4*Var(A) w.r.t. σ_A
    Normal A(10.0, 4.0);

    auto Sum = A + A;

    Expression var_sum = Sum->var_expr();
    var_sum->backward();

    // ∂(4σ²)/∂σ = 8σ = 8 * 2 = 16
    EXPECT_NEAR(A->std_expr()->gradient(), 16.0, 1e-10);
}

TEST_F(CovarianceExprTest, VarExpr_Sub_WithSharedInput) {
    // Var(A - A) = Var(A) - 2*Cov(A,A) + Var(A) = 0
    Normal A(10.0, 4.0);

    auto Diff = A - A;

    Expression var_diff = Diff->var_expr();

    EXPECT_NEAR(var_diff->value(), 0.0, 1e-10);
}

TEST_F(CovarianceExprTest, VarExpr_Max_Accuracy) {
    // Var(MAX(A,B)) calculated via cov_expr should match numerical variance
    Normal A(10.0, 4.0);
    Normal B(8.0, 1.0);

    auto MaxAB = MAX(A, B);

    Expression var_max = MaxAB->var_expr();

    // Note: Expression uses analytical formulas, RandomVariable uses tables/Gauss-Hermite
    // Actual error ~0.0001, tolerance set to 2e-4
    double expected_var = MaxAB->variance();
    EXPECT_NEAR(var_max->value(), expected_var, 2e-4);
}

TEST_F(CovarianceExprTest, VarExpr_Max_Gradient) {
    // Gradient of Var(MAX(A,B)) w.r.t. input parameters
    Normal A(10.0, 4.0);
    Normal B(8.0, 1.0);

    auto MaxAB = MAX(A, B);

    Expression var_max = MaxAB->var_expr();
    var_max->backward();

    std::cout << "\n=== Var(MAX) Gradients ===\n";
    std::cout << "Var(MAX(A,B)) = " << var_max->value() << "\n";
    std::cout << "∂Var/∂μ_A = " << A->mean_expr()->gradient() << "\n";
    std::cout << "∂Var/∂σ_A = " << A->std_expr()->gradient() << "\n";
    std::cout << "∂Var/∂μ_B = " << B->mean_expr()->gradient() << "\n";
    std::cout << "∂Var/∂σ_B = " << B->std_expr()->gradient() << "\n";

    // Variance should increase with σ_A (dominant variable)
    EXPECT_GT(A->std_expr()->gradient(), 0.0);
}

// ============================================================================
// Consistency with numerical covariance
// ============================================================================

TEST_F(CovarianceExprTest, ConsistencyCheck_ComplexPath) {
    // Complex path: Path = A + MAX(B, C + D)
    Normal A(10.0, 4.0);
    Normal B(5.0, 1.0);
    Normal C(6.0, 2.25);
    Normal D(3.0, 0.25);

    auto Sum_CD = C + D;
    auto Max_B_CD = MAX(B, Sum_CD);
    auto Path = A + Max_B_CD;

    // Check various covariances
    Expression cov_path_a = cov_expr(Path, A);
    Expression cov_path_b = cov_expr(Path, B);
    Expression cov_path_c = cov_expr(Path, C);

    double num_cov_path_a = covariance(Path, A);
    double num_cov_path_b = covariance(Path, B);
    double num_cov_path_c = covariance(Path, C);

    // Note: Complex paths accumulate numerical differences
    EXPECT_NEAR(cov_path_a->value(), num_cov_path_a, 1e-3);
    EXPECT_NEAR(cov_path_b->value(), num_cov_path_b, 1e-3);
    EXPECT_NEAR(cov_path_c->value(), num_cov_path_c, 1e-3);
}

TEST_F(CovarianceExprTest, ConsistencyCheck_PathVariance) {
    // Path variance should be consistent
    Normal A(10.0, 4.0);
    Normal B(5.0, 1.0);
    Normal C(6.0, 2.25);

    auto Path = A + MAX(B, C);

    Expression var_path = Path->var_expr();

    // Note: Complex variance calculations accumulate numerical differences
    // Actual error ~0.0075, tolerance set to 8e-3
    double expected_var = Path->variance();
    EXPECT_NEAR(var_path->value(), expected_var, 8e-3);
}

}  // namespace RandomVariable

