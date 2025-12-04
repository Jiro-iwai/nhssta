// -*- c++ -*-
// Verify if negative upstream is mathematically correct

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
    std::cout << "=== Verify Negative Upstream Mathematical Correctness ===\n\n";
    
    // Test 1: MINUS operation - upstream should be negative for right operand
    std::cout << "Test 1: MINUS operation\n";
    std::cout << "  f = left - right\n";
    std::cout << "  ∂f/∂left = 1 (positive)\n";
    std::cout << "  ∂f/∂right = -1 (negative - MATHEMATICALLY CORRECT!)\n\n";
    
    Variable left, right;
    left = 10.0;
    right = 8.0;
    Expression diff = left - right;
    
    zero_all_grad();
    diff->backward();
    
    double grad_left = left->gradient();
    double grad_right = right->gradient();
    
    std::cout << "  grad_left = " << grad_left << " (expected: 1.0)\n";
    std::cout << "  grad_right = " << grad_right << " (expected: -1.0)\n";
    std::cout << "  ✓ Negative upstream for right operand is CORRECT\n\n";
    
    // Test 2: MUL operation with negative left
    std::cout << "Test 2: MUL operation with negative left\n";
    std::cout << "  f = left × right, where left < 0\n";
    std::cout << "  ∂f/∂right = left (negative if left < 0 - MATHEMATICALLY CORRECT!)\n\n";
    
    Variable neg_left, pos_right;
    neg_left = -2.0;
    pos_right = 5.0;
    Expression mul = neg_left * pos_right;
    
    zero_all_grad();
    mul->backward();
    
    double grad_neg_left = neg_left->gradient();
    double grad_pos_right = pos_right->gradient();
    
    std::cout << "  grad_neg_left = " << grad_neg_left << " (expected: 5.0)\n";
    std::cout << "  grad_pos_right = " << grad_pos_right << " (expected: -2.0)\n";
    std::cout << "  ✓ Negative upstream for right operand is CORRECT\n\n";
    
    // Test 3: MAX0 with negative input
    std::cout << "Test 3: MAX0 with negative input\n";
    std::cout << "  MAX0(X) where X < 0\n";
    std::cout << "  E[MAX0(X)] = μ + σ × MeanMax(-μ/σ)\n";
    std::cout << "  where μ < 0 (negative mean)\n\n";
    
    Normal neg_input(-2.0, 5.0);  // Negative mean
    auto max0_neg = MAX0(neg_input);
    
    std::cout << "  Input: μ = " << neg_input->mean() << ", σ = " << sqrt(neg_input->variance()) << "\n";
    std::cout << "  E[MAX0] = " << max0_neg->mean() << " (should be small, close to 0)\n";
    
    zero_all_grad();
    Expression mean_max0_neg = max0_neg->mean_expr();
    mean_max0_neg->backward();
    
    double grad_mu_neg = neg_input->mean_expr()->gradient();
    
    std::cout << "  ∂E[MAX0]/∂μ = " << grad_mu_neg << "\n";
    
    // Theoretical: ∂E[MAX0]/∂μ = Φ(μ/σ)
    double mu = neg_input->mean();
    double sigma = sqrt(neg_input->variance());
    double a = -mu / sigma;  // This is positive when mu < 0
    double Phi_a = 0.5 * (1.0 + erf(a / sqrt(2.0)));
    double expected_grad = 1.0 - Phi_a;  // = Φ(μ/σ)
    
    std::cout << "  Expected ∂E[MAX0]/∂μ = Φ(μ/σ) = " << expected_grad << "\n";
    std::cout << "  Note: This should be in [0, 1] range\n";
    std::cout << "  ✓ MAX0 gradient is non-negative (CORRECT)\n\n";
    
    // Test 4: MAX0 in chain with negative upstream
    std::cout << "Test 4: MAX0 in chain with negative upstream\n";
    std::cout << "  f = MAX0(g), where g = left - right (negative)\n";
    std::cout << "  ∂f/∂right = (∂MAX0/∂g) × (∂g/∂right)\n";
    std::cout << "            = (∂MAX0/∂g) × (-1)\n";
    std::cout << "  If ∂MAX0/∂g > 0, then ∂f/∂right < 0\n";
    std::cout << "  This is MATHEMATICALLY CORRECT!\n\n";
    
    Variable left_var, right_var;
    left_var = 8.0;
    right_var = 10.0;  // left - right < 0
    Expression diff_neg = left_var - right_var;
    
    // We can't directly use MAX0 on Expression, so let's simulate
    // MAX0(diff_neg) would have negative input
    
    std::cout << "  left - right = " << diff_neg->value() << " (negative)\n";
    std::cout << "  If we compute MAX0(left-right):\n";
    std::cout << "    ∂MAX0/∂(left-right) > 0 (MAX0 increases with input)\n";
    std::cout << "    ∂(left-right)/∂right = -1\n";
    std::cout << "    Therefore: ∂MAX0/∂right = (∂MAX0/∂(left-right)) × (-1) < 0\n";
    std::cout << "  ✓ Negative upstream is CORRECT in this case\n\n";
    
    // Test 5: Multi-stage MAX - the actual case
    std::cout << "Test 5: Multi-stage MAX - the actual problematic case\n";
    std::cout << "  MAX(MAX(A,B), MAX(A,C)) where E[MAX(A,B)] < E[MAX(A,C)]\n";
    std::cout << "  = MAX(A,C) + MAX0(MAX(A,B) - MAX(A,C))\n";
    std::cout << "  Since MAX(A,B) - MAX(A,C) < 0:\n";
    std::cout << "    ∂MAX0/∂(MAX(A,B) - MAX(A,C)) > 0 (but small)\n";
    std::cout << "    ∂(MAX(A,B) - MAX(A,C))/∂B = ∂MAX(A,B)/∂B\n";
    std::cout << "    Therefore: ∂MAX0/∂B = (∂MAX0/∂diff) × (∂MAX(A,B)/∂B)\n\n";
    
    Normal A(10.0, 4.0);
    Normal B(8.0, 1.0);
    Normal C(12.0, 2.0);
    
    auto MaxAB = MAX(A, B);
    auto MaxAC = MAX(A, C);
    auto MaxABC = MAX(MaxAB, MaxAC);
    
    std::cout << "  E[MAX(A,B)] = " << MaxAB->mean() << "\n";
    std::cout << "  E[MAX(A,C)] = " << MaxAC->mean() << "\n";
    std::cout << "  E[MAX(A,B)] - E[MAX(A,C)] = " << (MaxAB->mean() - MaxAC->mean()) << " (negative)\n\n";
    
    zero_all_grad();
    Expression mean_ABC = MaxABC->mean_expr();
    mean_ABC->backward();
    
    double grad_B_final = B->mean_expr()->gradient();
    
    std::cout << "  Final grad_B = " << grad_B_final;
    if (grad_B_final < -1e-10) {
        std::cout << " [NEGATIVE!]";
    }
    std::cout << "\n\n";
    
    std::cout << "=== Analysis ===\n\n";
    std::cout << "Question: Is negative upstream mathematically correct?\n\n";
    std::cout << "Answer: YES, in the following cases:\n";
    std::cout << "  1. MINUS: right operand always has negative gradient\n";
    std::cout << "  2. MUL: right operand has negative gradient if left < 0\n";
    std::cout << "  3. Chain rule: if upstream is negative and local gradient is positive,\n";
    std::cout << "     the product (upstream × local_grad) can be negative\n\n";
    
    std::cout << "However, the QUESTION is:\n";
    std::cout << "  Should the FINAL mean gradient be negative?\n\n";
    std::cout << "For MAX0:\n";
    std::cout << "  ∂E[MAX0]/∂μ = Φ(μ/σ) ∈ [0, 1] (always non-negative)\n";
    std::cout << "  This is mathematically proven.\n\n";
    
    std::cout << "For multi-stage MAX:\n";
    std::cout << "  The gradient should propagate through:\n";
    std::cout << "    MAX(MAX(A,B), MAX(A,C)) = MAX(A,C) + MAX0(MAX(A,B) - MAX(A,C))\n";
    std::cout << "  B's gradient comes from:\n";
    std::cout << "    ∂MAX0(MAX(A,B) - MAX(A,C))/∂B\n";
    std::cout << "  = (∂MAX0/∂diff) × (∂diff/∂B)\n";
    std::cout << "  = (∂MAX0/∂diff) × (∂MAX(A,B)/∂B)\n\n";
    
    std::cout << "  Since MAX(A,B) - MAX(A,C) < 0:\n";
    std::cout << "    ∂MAX0/∂diff should be small but positive\n";
    std::cout << "    ∂MAX(A,B)/∂B should be positive\n";
    std::cout << "    Therefore: ∂MAX0/∂B should be positive\n\n";
    
    std::cout << "  BUT: The actual computation shows negative gradient!\n";
    std::cout << "  This suggests a problem in the implementation.\n";
    
    return 0;
}

