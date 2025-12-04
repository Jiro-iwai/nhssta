// -*- c++ -*-
// Trace final gradient contributions to understand why final mean gradient is negative

#include "../src/random_variable.hpp"
#include "../src/max.hpp"
#include "../src/expression.hpp"
#include "../src/normal.hpp"
#include <iostream>
#include <iomanip>
#include <cmath>
#include <vector>

using ::RandomVariable::Normal;
using ::RandomVariable::MAX;
using RandomVar = ::RandomVariable::RandomVariable;

struct GradientContribution {
    std::string source;
    double value;
    std::string description;
};

int main() {
    std::cout << std::fixed << std::setprecision(8);
    std::cout << "=== Trace Final Gradient Contributions ===\n\n";
    
    Normal A(10.0, 4.0);
    Normal B(8.0, 1.0);
    Normal C(12.0, 2.0);
    
    std::cout << "Input:\n";
    std::cout << "  A ~ N(" << A->mean() << ", " << A->variance() << ")\n";
    std::cout << "  B ~ N(" << B->mean() << ", " << B->variance() << ")\n";
    std::cout << "  C ~ N(" << C->mean() << ", " << C->variance() << ")\n\n";
    
    // Step 1: MAX(A, B)
    auto MaxAB = MAX(A, B);
    std::cout << "Step 1: MAX(A, B)\n";
    std::cout << "  E[MAX(A,B)] = " << MaxAB->mean() << "\n";
    
    zero_all_grad();
    Expression mean_AB = MaxAB->mean_expr();
    mean_AB->backward();
    
    double grad_A_from_AB = A->mean_expr()->gradient();
    double grad_B_from_AB = B->mean_expr()->gradient();
    
    std::cout << "  grad_A (from MAX(A,B)) = " << grad_A_from_AB << "\n";
    std::cout << "  grad_B (from MAX(A,B)) = " << grad_B_from_AB << "\n";
    std::cout << "  Sum = " << (grad_A_from_AB + grad_B_from_AB) << "\n\n";
    
    // Step 2: MAX(A, C)
    auto MaxAC = MAX(A, C);
    std::cout << "Step 2: MAX(A, C)\n";
    std::cout << "  E[MAX(A,C)] = " << MaxAC->mean() << "\n";
    
    zero_all_grad();
    Expression mean_AC = MaxAC->mean_expr();
    mean_AC->backward();
    
    double grad_A_from_AC = A->mean_expr()->gradient();
    double grad_C_from_AC = C->mean_expr()->gradient();
    
    std::cout << "  grad_A (from MAX(A,C)) = " << grad_A_from_AC << "\n";
    std::cout << "  grad_C (from MAX(A,C)) = " << grad_C_from_AC << "\n";
    std::cout << "  Sum = " << (grad_A_from_AC + grad_C_from_AC) << "\n\n";
    
    // Step 3: MAX(MAX(A,B), MAX(A,C))
    auto MaxABC = MAX(MaxAB, MaxAC);
    std::cout << "Step 3: MAX(MAX(A,B), MAX(A,C))\n";
    std::cout << "  E[MAX(MAX(A,B), MAX(A,C))] = " << MaxABC->mean() << "\n\n";
    
    zero_all_grad();
    Expression mean_ABC = MaxABC->mean_expr();
    mean_ABC->backward();
    
    double grad_A_final = A->mean_expr()->gradient();
    double grad_B_final = B->mean_expr()->gradient();
    double grad_C_final = C->mean_expr()->gradient();
    
    std::cout << "Final gradients:\n";
    std::cout << "  grad_A (final) = " << grad_A_final << "\n";
    std::cout << "  grad_B (final) = " << grad_B_final;
    if (grad_B_final < -1e-10) {
        std::cout << " [NEGATIVE!]";
    }
    std::cout << "\n";
    std::cout << "  grad_C (final) = " << grad_C_final << "\n";
    std::cout << "  Sum = " << (grad_A_final + grad_B_final + grad_C_final) << "\n\n";
    
    // Analyze: B's gradient comes from MAX(A,B) path
    // But MAX(A,B) is normalized to A + MAX0(B-A)
    // So B's gradient comes from MAX0(B-A)
    
    std::cout << "=== Analysis: Why is grad_B negative? ===\n\n";
    
    std::cout << "MAX(A,B) is normalized to: A + MAX0(B-A)\n";
    std::cout << "  Since mu_A = 10.0 > mu_B = 8.0\n";
    std::cout << "  MAX(A,B) = A + MAX0(B-A)\n\n";
    
    std::cout << "B's gradient comes from MAX0(B-A):\n";
    std::cout << "  B-A ~ N(" << (B->mean() - A->mean()) << ", " << (B->variance() + A->variance()) << ")\n";
    std::cout << "  E[MAX0(B-A)] = " << MAX0(B - A)->mean() << "\n";
    
    // Check MAX0(B-A) gradient
    auto diff_BA = B - A;
    auto max0_BA = MAX0(diff_BA);
    
    zero_all_grad();
    Expression mean_max0_BA = max0_BA->mean_expr();
    mean_max0_BA->backward();
    
    double grad_B_from_max0 = B->mean_expr()->gradient();
    double grad_A_from_max0 = A->mean_expr()->gradient();
    
    std::cout << "  ∂E[MAX0(B-A)]/∂μ_B = " << grad_B_from_max0 << "\n";
    std::cout << "  ∂E[MAX0(B-A)]/∂μ_A = " << grad_A_from_max0 << "\n";
    std::cout << "  Sum = " << (grad_B_from_max0 + grad_A_from_max0) << "\n\n";
    
    std::cout << "Theoretical check:\n";
    std::cout << "  MAX0(B-A) mean: E[MAX0(B-A)] = (μ_B-μ_A) + σ × MeanMax(-(μ_B-μ_A)/σ)\n";
    std::cout << "  ∂E[MAX0(B-A)]/∂μ_B = 1 + σ × (∂MeanMax/∂a) × (∂a/∂μ_B)\n";
    std::cout << "                      = 1 + σ × Φ(a) × (-1/σ)\n";
    std::cout << "                      = 1 - Φ(a)\n";
    std::cout << "                      = 1 - Φ(-(μ_B-μ_A)/σ)\n";
    std::cout << "                      = Φ((μ_B-μ_A)/σ)\n";
    
    double mu_diff = B->mean() - A->mean();
    double sigma_diff = sqrt(B->variance() + A->variance());
    double a = -mu_diff / sigma_diff;
    double Phi_a = 0.5 * (1.0 + erf(a / sqrt(2.0)));
    double expected_grad_B = 1.0 - Phi_a;
    
    std::cout << "  Expected ∂E[MAX0(B-A)]/∂μ_B = 1 - Φ(" << a << ") = " << expected_grad_B << "\n";
    std::cout << "  Actual ∂E[MAX0(B-A)]/∂μ_B = " << grad_B_from_max0 << "\n";
    std::cout << "  Difference: " << (grad_B_from_max0 - expected_grad_B) << "\n\n";
    
    std::cout << "Now, in MAX(MAX(A,B), MAX(A,C)):\n";
    std::cout << "  B's final gradient comes from:\n";
    std::cout << "    1. MAX(A,B) path: contributes grad_B_from_AB\n";
    std::cout << "    2. But MAX(A,B) is part of MAX(MAX(A,B), MAX(A,C))\n";
    std::cout << "    3. The gradient propagates through the outer MAX\n\n";
    
    std::cout << "Key insight: The outer MAX might reduce B's contribution\n";
    std::cout << "  because MAX(A,C) dominates (mu_C = 12.0 > mu_A = 10.0)\n";
    
    return 0;
}

