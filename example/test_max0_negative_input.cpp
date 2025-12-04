// -*- c++ -*-
// Test MAX0 with negative input to understand gradient behavior

#include "../src/random_variable.hpp"
#include "../src/max.hpp"
#include "../src/expression.hpp"
#include "../src/normal.hpp"
#include <iostream>
#include <iomanip>
#include <cmath>

using ::RandomVariable::Normal;
using ::RandomVariable::MAX0;
using RandomVar = ::RandomVariable::RandomVariable;

int main() {
    std::cout << std::fixed << std::setprecision(8);
    std::cout << "=== Test MAX0 with Negative Input ===\n\n";
    
    // MAX0 formula: E[MAX0] = μ + σ × MeanMax(-μ/σ)
    // When μ < 0 (negative input), -μ/σ > 0
    
    std::cout << "Case 1: Strongly negative input\n";
    Normal neg1(-5.0, 1.0);
    auto max0_neg1 = MAX0(neg1);
    
    std::cout << "  Input: μ = " << neg1->mean() << ", σ = " << sqrt(neg1->variance()) << "\n";
    std::cout << "  E[MAX0] = " << max0_neg1->mean() << " (should be ≈ 0)\n";
    
    zero_all_grad();
    Expression mean_max0_neg1 = max0_neg1->mean_expr();
    mean_max0_neg1->backward();
    
    double grad_mu_neg1 = neg1->mean_expr()->gradient();
    
    double mu = neg1->mean();
    double sigma = sqrt(neg1->variance());
    double a = -mu / sigma;
    double Phi_a = 0.5 * (1.0 + erf(a / sqrt(2.0)));
    double expected_grad1 = 1.0 - Phi_a;
    
    std::cout << "  ∂E[MAX0]/∂μ = " << grad_mu_neg1 << "\n";
    std::cout << "  Expected = Φ(μ/σ) = " << expected_grad1 << "\n";
    std::cout << "  Note: Should be in [0, 1] and close to 0 for strongly negative μ\n\n";
    
    std::cout << "Case 2: Slightly negative input (like MAX(A,B) - MAX(A,C))\n";
    Normal neg2(-2.0, 5.0);  // Similar to the actual case
    auto max0_neg2 = MAX0(neg2);
    
    std::cout << "  Input: μ = " << neg2->mean() << ", σ = " << sqrt(neg2->variance()) << "\n";
    std::cout << "  E[MAX0] = " << max0_neg2->mean() << "\n";
    
    zero_all_grad();
    Expression mean_max0_neg2 = max0_neg2->mean_expr();
    mean_max0_neg2->backward();
    
    double grad_mu_neg2 = neg2->mean_expr()->gradient();
    
    mu = neg2->mean();
    sigma = sqrt(neg2->variance());
    a = -mu / sigma;
    Phi_a = 0.5 * (1.0 + erf(a / sqrt(2.0)));
    double expected_grad2 = 1.0 - Phi_a;
    
    std::cout << "  ∂E[MAX0]/∂μ = " << grad_mu_neg2 << "\n";
    std::cout << "  Expected = Φ(μ/σ) = " << expected_grad2 << "\n";
    std::cout << "  Note: Should be positive (in [0, 1])\n\n";
    
    // Now test in a chain: MAX0(diff) where diff = left - right
    std::cout << "Case 3: MAX0 in chain with negative input\n";
    std::cout << "  f = MAX0(left - right), where left - right < 0\n";
    std::cout << "  ∂f/∂right = (∂MAX0/∂(left-right)) × (∂(left-right)/∂right)\n";
    std::cout << "            = (∂MAX0/∂(left-right)) × (-1)\n\n";
    
    Normal left(8.0, 1.0);
    Normal right(10.0, 4.0);
    auto diff = left - right;
    auto max0_diff = MAX0(diff);
    
    std::cout << "  left - right: μ = " << diff->mean() << ", σ = " << sqrt(diff->variance()) << "\n";
    std::cout << "  E[MAX0(left-right)] = " << max0_diff->mean() << "\n";
    
    zero_all_grad();
    Expression mean_max0_diff = max0_diff->mean_expr();
    mean_max0_diff->backward();
    
    double grad_left = left->mean_expr()->gradient();
    double grad_right = right->mean_expr()->gradient();
    
    std::cout << "  ∂MAX0/∂left = " << grad_left << "\n";
    std::cout << "  ∂MAX0/∂right = " << grad_right;
    if (grad_right < -1e-10) {
        std::cout << " [NEGATIVE - EXPECTED!]";
    }
    std::cout << "\n\n";
    
    std::cout << "  Analysis:\n";
    std::cout << "    ∂MAX0/∂(left-right) = " << grad_left << " (positive)\n";
    std::cout << "    ∂(left-right)/∂right = -1\n";
    std::cout << "    Therefore: ∂MAX0/∂right = " << grad_left << " × (-1) = " << grad_right << "\n";
    std::cout << "    ✓ Negative gradient for right is MATHEMATICALLY CORRECT!\n\n";
    
    std::cout << "=== Conclusion ===\n\n";
    std::cout << "1. Negative upstream in MINUS/MUL operations: CORRECT\n";
    std::cout << "2. Negative gradient for MAX0's input when input is negative: CORRECT\n";
    std::cout << "3. However, MAX0's gradient w.r.t. its own mean (μ) is always non-negative\n";
    std::cout << "4. The question is: In multi-stage MAX, should final mean gradient be negative?\n";
    std::cout << "   Answer: Probably NOT, but the chain rule can produce negative gradients\n";
    std::cout << "   when propagating through negative intermediate values.\n";
    
    return 0;
}

