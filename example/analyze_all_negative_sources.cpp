// -*- c++ -*-
// Analyze all sources of negative gradients, not just MUL

#include "../src/random_variable.hpp"
#include "../src/max.hpp"
#include "../src/expression.hpp"
#include "../src/normal.hpp"
#include <iostream>
#include <iomanip>
#include <cmath>

using ::RandomVariable::Normal;
using ::RandomVariable::MAX;
using RandomVar = ::RandomVariable::RandomVariable;

int main() {
    std::cout << std::fixed << std::setprecision(8);
    std::cout << "=== Analyze All Negative Gradient Sources ===\n\n";
    
    // Test 1: MINUS operation
    std::cout << "Test 1: MINUS operation\n";
    std::cout << "  f = A - B\n";
    std::cout << "  ∂f/∂A = 1 (always positive)\n";
    std::cout << "  ∂f/∂B = -1 (always negative!)\n\n";
    
    Variable A, B;
    A = 10.0;
    B = 8.0;
    Expression diff = A - B;
    
    zero_all_grad();
    diff->backward();
    
    double grad_A = A->gradient();
    double grad_B = B->gradient();
    
    std::cout << "  grad_A = " << grad_A << "\n";
    std::cout << "  grad_B = " << grad_B;
    if (grad_B < -1e-10) {
        std::cout << " [NEGATIVE - EXPECTED!]";
    }
    std::cout << "\n\n";
    
    // Test 2: DIV operation
    std::cout << "Test 2: DIV operation\n";
    std::cout << "  f = A / B\n";
    std::cout << "  ∂f/∂A = 1/B (positive if B > 0)\n";
    std::cout << "  ∂f/∂B = -A/B² (negative if A > 0 and B > 0!)\n\n";
    
    A = 10.0;
    B = 2.0;
    Expression div = A / B;
    
    zero_all_grad();
    div->backward();
    
    grad_A = A->gradient();
    grad_B = B->gradient();
    
    std::cout << "  grad_A = " << grad_A << "\n";
    std::cout << "  grad_B = " << grad_B;
    if (grad_B < -1e-10) {
        std::cout << " [NEGATIVE - EXPECTED!]";
    }
    std::cout << "\n";
    std::cout << "  Expected grad_B = -A/B² = " << (-A->value() / (B->value() * B->value())) << "\n\n";
    
    // Test 3: MUL operation with negative value
    std::cout << "Test 3: MUL operation with negative left value\n";
    std::cout << "  f = (-1) × B\n";
    std::cout << "  ∂f/∂B = -1 (negative!)\n\n";
    
    Variable neg_one;
    neg_one = -1.0;
    B = 5.0;
    Expression mul_neg = neg_one * B;
    
    zero_all_grad();
    mul_neg->backward();
    
    double grad_neg_one = neg_one->gradient();
    grad_B = B->gradient();
    
    std::cout << "  grad_neg_one = " << grad_neg_one << "\n";
    std::cout << "  grad_B = " << grad_B;
    if (grad_B < -1e-10) {
        std::cout << " [NEGATIVE - EXPECTED!]";
    }
    std::cout << "\n\n";
    
    // Test 4: Chain of operations leading to negative gradient
    std::cout << "Test 4: Chain: -(A/B) × C\n";
    std::cout << "  f = -(A/B) × C = -A×C/B\n";
    std::cout << "  This involves DIV (negative grad_B) and MUL (negative left value)\n\n";
    
    A = 10.0;
    B = 2.0;
    Variable C;
    C = 3.0;
    Expression chain = -(A / B) * C;
    
    zero_all_grad();
    chain->backward();
    
    grad_A = A->gradient();
    grad_B = B->gradient();
    double grad_C = C->gradient();
    
    std::cout << "  grad_A = " << grad_A << "\n";
    std::cout << "  grad_B = " << grad_B;
    if (grad_B < -1e-10) {
        std::cout << " [NEGATIVE - from DIV!]";
    }
    std::cout << "\n";
    std::cout << "  grad_C = " << grad_C;
    if (grad_C < -1e-10) {
        std::cout << " [NEGATIVE - from MUL with negative left!]";
    }
    std::cout << "\n\n";
    
    // Test 5: MAX0 with negative mean (like B-A when mu_B < mu_A)
    std::cout << "Test 5: MAX0 with negative mean\n";
    std::cout << "  This is the actual case in MAX(MAX(A,B), MAX(A,C))\n";
    std::cout << "  When mu_B < mu_A, MAX(A,B) = A + MAX0(B-A)\n";
    std::cout << "  B-A has negative mean, so -(B-A)/σ is positive\n";
    std::cout << "  But the expression tree has -(μ/σ) which involves MINUS\n\n";
    
    Normal A_norm(10.0, 4.0);
    Normal B_norm(8.0, 1.0);
    auto diff_AB = B_norm - A_norm;  // B - A has negative mean
    auto max0_diff = MAX0(diff_AB);
    
    zero_all_grad();
    Expression mean_max0 = max0_diff->mean_expr();
    mean_max0->backward();
    
    double grad_A_mean = A_norm->mean_expr()->gradient();
    double grad_B_mean = B_norm->mean_expr()->gradient();
    
    std::cout << "  grad_A_mean = " << grad_A_mean << "\n";
    std::cout << "  grad_B_mean = " << grad_B_mean << "\n";
    std::cout << "  Note: grad_B_mean should be positive (MAX0 increases with mean)\n";
    std::cout << "        grad_A_mean should be negative (MAX0 decreases as A increases)\n\n";
    
    std::cout << "=== Summary ===\n";
    std::cout << "Negative gradients can occur in:\n";
    std::cout << "  1. MINUS: right operand always has negative gradient\n";
    std::cout << "  2. DIV: right operand has negative gradient if numerator > 0\n";
    std::cout << "  3. MUL: right operand has negative gradient if left < 0\n";
    std::cout << "  4. CUSTOM_FUNCTION: can propagate negative gradients from above\n";
    std::cout << "\nAll of these are mathematically correct!\n";
    std::cout << "The question is: should final mean gradients be negative?\n";
    
    return 0;
}

