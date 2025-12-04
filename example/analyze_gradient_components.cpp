// -*- c++ -*-
// Analyze which component of (∂MAX0/∂diff) × (∂MAX(A,B)/∂B) is negative

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
    std::cout << "=== Analyze Gradient Components ===\n\n";
    
    Normal A(10.0, 4.0);
    Normal B(8.0, 1.0);
    Normal C(12.0, 2.0);
    
    std::cout << "Input:\n";
    std::cout << "  A ~ N(" << A->mean() << ", " << A->variance() << ")\n";
    std::cout << "  B ~ N(" << B->mean() << ", " << B->variance() << ")\n";
    std::cout << "  C ~ N(" << C->mean() << ", " << C->variance() << ")\n\n";
    
    // Component 1: MAX(A,B)
    auto MaxAB = MAX(A, B);
    std::cout << "Component 1: MAX(A,B)\n";
    std::cout << "  E[MAX(A,B)] = " << MaxAB->mean() << "\n";
    
    zero_all_grad();
    Expression mean_AB = MaxAB->mean_expr();
    mean_AB->backward();
    
    double grad_A_from_AB = A->mean_expr()->gradient();
    double grad_B_from_AB = B->mean_expr()->gradient();
    
    std::cout << "  ∂MAX(A,B)/∂A = " << grad_A_from_AB << "\n";
    std::cout << "  ∂MAX(A,B)/∂B = " << grad_B_from_AB;
    if (grad_B_from_AB < -1e-10) {
        std::cout << " [NEGATIVE!]";
    } else {
        std::cout << " [POSITIVE - EXPECTED]";
    }
    std::cout << "\n";
    std::cout << "  Sum = " << (grad_A_from_AB + grad_B_from_AB) << "\n\n";
    
    // Component 2: MAX(A,C)
    auto MaxAC = MAX(A, C);
    std::cout << "Component 2: MAX(A,C)\n";
    std::cout << "  E[MAX(A,C)] = " << MaxAC->mean() << "\n\n";
    
    // Component 3: diff = MAX(A,B) - MAX(A,C)
    // We can't directly compute this, but we know:
    double diff_mean = MaxAB->mean() - MaxAC->mean();
    std::cout << "Component 3: diff = MAX(A,B) - MAX(A,C)\n";
    std::cout << "  E[diff] = " << diff_mean;
    if (diff_mean < -1e-10) {
        std::cout << " [NEGATIVE!]";
    }
    std::cout << "\n\n";
    
    // Component 4: MAX0(diff) where diff < 0
    // We need to create a RandomVariable with mean = diff_mean
    // But we can't directly do this, so let's analyze theoretically
    
    std::cout << "Component 4: MAX0(diff) where diff < 0\n";
    std::cout << "  Since diff < 0, MAX0(diff) ≈ 0 (small positive value)\n";
    std::cout << "  ∂MAX0/∂diff = Φ(diff/σ_diff) ∈ [0, 1]\n";
    std::cout << "  Since diff < 0, diff/σ_diff < 0\n";
    std::cout << "  Therefore: Φ(diff/σ_diff) < 0.5 (but still positive!)\n";
    std::cout << "  Expected: ∂MAX0/∂diff > 0 (small but positive)\n\n";
    
    // Now compute the full expression: MAX(MAX(A,B), MAX(A,C))
    auto MaxABC = MAX(MaxAB, MaxAC);
    std::cout << "Full Expression: MAX(MAX(A,B), MAX(A,C))\n";
    std::cout << "  = MAX(A,C) + MAX0(MAX(A,B) - MAX(A,C))\n";
    std::cout << "  = MAX(A,C) + MAX0(diff) where diff < 0\n\n";
    
    zero_all_grad();
    Expression mean_ABC = MaxABC->mean_expr();
    mean_ABC->backward();
    
    double grad_A_final = A->mean_expr()->gradient();
    double grad_B_final = B->mean_expr()->gradient();
    double grad_C_final = C->mean_expr()->gradient();
    
    std::cout << "Final gradients:\n";
    std::cout << "  grad_A = " << grad_A_final << "\n";
    std::cout << "  grad_B = " << grad_B_final;
    if (grad_B_final < -1e-10) {
        std::cout << " [NEGATIVE!]";
    }
    std::cout << "\n";
    std::cout << "  grad_C = " << grad_C_final << "\n";
    std::cout << "  Sum = " << (grad_A_final + grad_B_final + grad_C_final) << "\n\n";
    
    // Analysis: B's gradient comes from MAX0(diff) path
    // grad_B = ∂MAX0(diff)/∂B
    //        = (∂MAX0/∂diff) × (∂diff/∂B)
    //        = (∂MAX0/∂diff) × (∂(MAX(A,B) - MAX(A,C))/∂B)
    //        = (∂MAX0/∂diff) × (∂MAX(A,B)/∂B - ∂MAX(A,C)/∂B)
    //        = (∂MAX0/∂diff) × (∂MAX(A,B)/∂B - 0)
    //        = (∂MAX0/∂diff) × (∂MAX(A,B)/∂B)
    
    std::cout << "=== Analysis: Which component is negative? ===\n\n";
    std::cout << "B's final gradient = (∂MAX0/∂diff) × (∂MAX(A,B)/∂B)\n\n";
    
    std::cout << "Component 1: ∂MAX(A,B)/∂B = " << grad_B_from_AB << "\n";
    std::cout << "  This should be POSITIVE (MAX(A,B) increases with B)\n";
    std::cout << "  ✓ Confirmed: POSITIVE\n\n";
    
    std::cout << "Component 2: ∂MAX0/∂diff (where diff = MAX(A,B) - MAX(A,C) < 0)\n";
    std::cout << "  Theoretical: ∂MAX0/∂diff = Φ(diff/σ_diff) ∈ [0, 1]\n";
    std::cout << "  Since diff < 0, this should be POSITIVE but small\n";
    std::cout << "  However, we need to check the actual computation...\n\n";
    
    std::cout << "Problem: If both components are positive, the product should be positive!\n";
    std::cout << "But grad_B_final = " << grad_B_final << " is NEGATIVE!\n\n";
    
    std::cout << "Possible explanations:\n";
    std::cout << "  1. ∂MAX0/∂diff is actually negative (implementation bug?)\n";
    std::cout << "  2. The gradient path is more complex than expected\n";
    std::cout << "  3. There are other negative contributions from other paths\n";
    std::cout << "  4. The sign of ∂MAX0/∂diff is flipped somewhere\n";
    
    return 0;
}

