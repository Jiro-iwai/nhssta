// -*- c++ -*-
// Analyze the path that leads to negative final gradient

#include "../src/random_variable.hpp"
#include "../src/max.hpp"
#include "../src/expression.hpp"
#include "../src/normal.hpp"
#include <iostream>
#include <iomanip>
#include <cmath>

using ::RandomVariable::Normal;
using ::RandomVariable::MAX;
using ::RandomVariable::MAX0;
using RandomVar = ::RandomVariable::RandomVariable;

int main() {
    std::cout << std::fixed << std::setprecision(8);
    std::cout << "=== Analyze Final Gradient Path ===\n\n";
    
    Normal A(10.0, 4.0);
    Normal B(8.0, 1.0);
    Normal C(12.0, 2.0);
    
    std::cout << "Input:\n";
    std::cout << "  A ~ N(" << A->mean() << ", " << A->variance() << ")\n";
    std::cout << "  B ~ N(" << B->mean() << ", " << B->variance() << ")\n";
    std::cout << "  C ~ N(" << C->mean() << ", " << C->variance() << ")\n\n";
    
    // The key: MAX(A,B) when mu_A >= mu_B
    // MAX(A,B) = A + MAX0(B-A)
    // B's gradient comes ONLY from MAX0(B-A)
    
    std::cout << "=== Path 1: MAX(A,B) = A + MAX0(B-A) ===\n";
    std::cout << "  Since mu_A = 10.0 >= mu_B = 8.0\n";
    std::cout << "  MAX(A,B) = A + MAX0(B-A)\n\n";
    
    // Compute B-A
    auto diff_BA = B - A;
    std::cout << "  B-A ~ N(" << diff_BA->mean() << ", " << diff_BA->variance() << ")\n";
    
    auto max0_BA = MAX0(diff_BA);
    std::cout << "  E[MAX0(B-A)] = " << max0_BA->mean() << "\n";
    
    zero_all_grad();
    Expression mean_max0_BA = max0_BA->mean_expr();
    mean_max0_BA->backward();
    
    double grad_B_from_max0_BA = B->mean_expr()->gradient();
    double grad_A_from_max0_BA = A->mean_expr()->gradient();
    
    std::cout << "  ∂E[MAX0(B-A)]/∂μ_B = " << grad_B_from_max0_BA << "\n";
    std::cout << "  ∂E[MAX0(B-A)]/∂μ_A = " << grad_A_from_max0_BA << "\n";
    std::cout << "  Note: grad_B should be positive (MAX0 increases with mean)\n";
    std::cout << "        grad_A should be negative (MAX0 decreases as A increases)\n\n";
    
    // Now MAX(A,B) = A + MAX0(B-A)
    auto MaxAB = MAX(A, B);
    std::cout << "  E[MAX(A,B)] = " << MaxAB->mean() << "\n";
    
    zero_all_grad();
    Expression mean_AB = MaxAB->mean_expr();
    mean_AB->backward();
    
    double grad_B_from_AB = B->mean_expr()->gradient();
    double grad_A_from_AB = A->mean_expr()->gradient();
    
    std::cout << "  ∂E[MAX(A,B)]/∂μ_B = " << grad_B_from_AB << "\n";
    std::cout << "  ∂E[MAX(A,B)]/∂μ_A = " << grad_A_from_AB << "\n";
    std::cout << "  Sum = " << (grad_B_from_AB + grad_A_from_AB) << "\n\n";
    
    // Now MAX(A,C)
    std::cout << "=== Path 2: MAX(A,C) = C + MAX0(A-C) ===\n";
    std::cout << "  Since mu_C = 12.0 > mu_A = 10.0\n";
    std::cout << "  MAX(A,C) = C + MAX0(A-C)\n\n";
    
    auto diff_AC = A - C;
    std::cout << "  A-C ~ N(" << diff_AC->mean() << ", " << diff_AC->variance() << ")\n";
    
    auto MaxAC = MAX(A, C);
    std::cout << "  E[MAX(A,C)] = " << MaxAC->mean() << "\n";
    
    zero_all_grad();
    Expression mean_AC = MaxAC->mean_expr();
    mean_AC->backward();
    
    double grad_A_from_AC = A->mean_expr()->gradient();
    double grad_C_from_AC = C->mean_expr()->gradient();
    
    std::cout << "  ∂E[MAX(A,C)]/∂μ_A = " << grad_A_from_AC << "\n";
    std::cout << "  ∂E[MAX(A,C)]/∂μ_C = " << grad_C_from_AC << "\n";
    std::cout << "  Sum = " << (grad_A_from_AC + grad_C_from_AC) << "\n\n";
    
    // Now MAX(MAX(A,B), MAX(A,C))
    std::cout << "=== Path 3: MAX(MAX(A,B), MAX(A,C)) ===\n";
    std::cout << "  E[MAX(A,B)] = " << MaxAB->mean() << "\n";
    std::cout << "  E[MAX(A,C)] = " << MaxAC->mean() << "\n";
    std::cout << "  Since E[MAX(A,C)] > E[MAX(A,B)]\n";
    std::cout << "  MAX(MAX(A,B), MAX(A,C)) = MAX(A,C) + MAX0(MAX(A,B) - MAX(A,C))\n\n";
    
    auto MaxABC = MAX(MaxAB, MaxAC);
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
    
    std::cout << "=== Analysis: Why is grad_B negative? ===\n\n";
    std::cout << "B's gradient comes from:\n";
    std::cout << "  1. MAX(A,B) path: contributes " << grad_B_from_AB << " (positive)\n";
    std::cout << "  2. But MAX(A,B) is part of MAX(MAX(A,B), MAX(A,C))\n";
    std::cout << "  3. The outer MAX normalizes: MAX(A,C) + MAX0(MAX(A,B) - MAX(A,C))\n";
    std::cout << "  4. B's contribution goes through MAX0(MAX(A,B) - MAX(A,C))\n";
    std::cout << "  5. Since MAX(A,B) < MAX(A,C), MAX(A,B) - MAX(A,C) < 0\n";
    std::cout << "  6. MAX0(negative) has negative gradient w.r.t. the negative input\n";
    std::cout << "  7. This negative gradient propagates back to B\n\n";
    
    std::cout << "The key insight:\n";
    std::cout << "  MAX0(MAX(A,B) - MAX(A,C)) where MAX(A,B) - MAX(A,C) < 0\n";
    std::cout << "  The gradient of MAX0 w.r.t. its input (which is negative) is negative\n";
    std::cout << "  This negative gradient propagates through the expression tree to B\n";
    
    return 0;
}

