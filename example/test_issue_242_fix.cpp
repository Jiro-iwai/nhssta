// Test for Issue #242: Verify that the smaller-base MAX fix resolves negative gradients
#include "../src/random_variable.hpp"
#include "../src/normal.hpp"
#include "../src/max.hpp"
#include "../src/expression.hpp"
#include <iostream>
#include <iomanip>

using namespace RandomVariable;

int main() {
    std::cout << "==================================================" << std::endl;
    std::cout << "Issue #242 Fix Verification" << std::endl;
    std::cout << "==================================================" << std::endl;
    std::cout << std::endl;
    
    // Test case from Issue #242
    Normal A(10.0, 4.0);
    Normal B(8.0, 1.0);
    Normal C(12.0, 2.0);
    
    std::cout << "Test: MAX(MAX(A,B), MAX(A,C))" << std::endl;
    std::cout << "A ~ N(10.0, 4.0)" << std::endl;
    std::cout << "B ~ N(8.0, 1.0)" << std::endl;
    std::cout << "C ~ N(12.0, 2.0)" << std::endl;
    std::cout << std::endl;
    
    auto MaxAB = MAX(A, B);
    auto MaxAC = MAX(A, C);
    auto MaxABC = MAX(MaxAB, MaxAC);
    
    // Compute mean
    double mean_val = MaxABC->mean();
    std::cout << "E[MAX(MAX(A,B), MAX(A,C))] = " << std::setprecision(8) << mean_val << std::endl;
    std::cout << std::endl;
    
    // Gradient computation
    zero_all_grad();
    Expression mean_expr = MaxABC->mean_expr();
    mean_expr->backward();
    
    double grad_A = A->mean_expr()->gradient();
    double grad_B = B->mean_expr()->gradient();
    double grad_C = C->mean_expr()->gradient();
    double sum = grad_A + grad_B + grad_C;
    
    std::cout << "Gradients:" << std::endl;
    std::cout << "  grad_A = " << std::setprecision(8) << grad_A;
    if (grad_A < 0) std::cout << " [NEGATIVE!]";
    std::cout << std::endl;
    
    std::cout << "  grad_B = " << std::setprecision(8) << grad_B;
    if (grad_B < 0) {
        std::cout << " [NEGATIVE!] ✗ FAILED";
    } else {
        std::cout << " [POSITIVE] ✓ PASSED";
    }
    std::cout << std::endl;
    
    std::cout << "  grad_C = " << std::setprecision(8) << grad_C;
    if (grad_C < 0) std::cout << " [NEGATIVE!]";
    std::cout << std::endl;
    
    std::cout << "  Sum = " << std::setprecision(8) << sum << std::endl;
    std::cout << std::endl;
    
    // Compare with Monte Carlo results
    std::cout << "==================================================" << std::endl;
    std::cout << "Comparison with Monte Carlo (Expected)" << std::endl;
    std::cout << "==================================================" << std::endl;
    std::cout << "Monte Carlo: grad_B = 0.00421200 (positive)" << std::endl;
    std::cout << "This fix:    grad_B = " << std::setprecision(8) << grad_B;
    if (grad_B > 0) {
        std::cout << " (positive) ✓" << std::endl;
    } else {
        std::cout << " (negative) ✗" << std::endl;
    }
    std::cout << std::endl;
    
    // Monotonicity check
    bool all_non_negative = (grad_A >= 0) && (grad_B >= 0) && (grad_C >= 0);
    std::cout << "==================================================" << std::endl;
    std::cout << "Monotonicity Check" << std::endl;
    std::cout << "==================================================" << std::endl;
    std::cout << "All gradients non-negative: ";
    if (all_non_negative) {
        std::cout << "✓ PASSED" << std::endl;
    } else {
        std::cout << "✗ FAILED" << std::endl;
    }
    std::cout << "Gradient sum equals 1: ";
    if (std::abs(sum - 1.0) < 1e-6) {
        std::cout << "✓ PASSED" << std::endl;
    } else {
        std::cout << "✗ FAILED (sum = " << sum << ")" << std::endl;
    }
    std::cout << std::endl;
    
    return all_non_negative ? 0 : 1;
}
