// -*- c++ -*-
// Verify MAX gradient theory: Should gradients always be positive?

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
    std::cout << "=== Verify MAX Gradient Theory ===\n\n";
    
    std::cout << "Theory: MAX is a monotonically increasing function\n";
    std::cout << "  If input increases, output increases (or stays same)\n";
    std::cout << "  Therefore: ∂MAX/∂input ≥ 0 (always non-negative)\n\n";
    
    std::cout << "For MAX(A, B):\n";
    std::cout << "  ∂MAX(A,B)/∂A ≥ 0\n";
    std::cout << "  ∂MAX(A,B)/∂B ≥ 0\n";
    std::cout << "  ∂MAX(A,B)/∂A + ∂MAX(A,B)/∂B = 1\n\n";
    
    std::cout << "For MAX(MAX(A,B), MAX(A,C)):\n";
    std::cout << "  ∂MAX/∂A ≥ 0\n";
    std::cout << "  ∂MAX/∂B ≥ 0\n";
    std::cout << "  ∂MAX/∂C ≥ 0\n";
    std::cout << "  ∂MAX/∂A + ∂MAX/∂B + ∂MAX/∂C = 1\n\n";
    
    Normal A(10.0, 4.0);
    Normal B(8.0, 1.0);
    Normal C(12.0, 2.0);
    
    auto MaxAB = MAX(A, B);
    auto MaxAC = MAX(A, C);
    auto MaxABC = MAX(MaxAB, MaxAC);
    
    zero_all_grad();
    Expression mean_expr = MaxABC->mean_expr();
    mean_expr->backward();
    
    double grad_A = A->mean_expr()->gradient();
    double grad_B = B->mean_expr()->gradient();
    double grad_C = C->mean_expr()->gradient();
    
    std::cout << "Actual results:\n";
    std::cout << "  grad_A = " << grad_A;
    if (grad_A < -1e-10) {
        std::cout << " [NEGATIVE - VIOLATES THEORY!]";
    } else {
        std::cout << " [OK]";
    }
    std::cout << "\n";
    
    std::cout << "  grad_B = " << grad_B;
    if (grad_B < -1e-10) {
        std::cout << " [NEGATIVE - VIOLATES THEORY!]";
    } else {
        std::cout << " [OK]";
    }
    std::cout << "\n";
    
    std::cout << "  grad_C = " << grad_C;
    if (grad_C < -1e-10) {
        std::cout << " [NEGATIVE - VIOLATES THEORY!]";
    } else {
        std::cout << " [OK]";
    }
    std::cout << "\n";
    
    std::cout << "  Sum = " << (grad_A + grad_B + grad_C) << "\n\n";
    
    std::cout << "=== Conclusion ===\n\n";
    
    if (grad_B < -1e-10) {
        std::cout << "❌ THEORY VIOLATION DETECTED!\n";
        std::cout << "   grad_B = " << grad_B << " < 0\n";
        std::cout << "   This violates the mathematical property that MAX is monotonically increasing.\n\n";
        
        std::cout << "Possible causes:\n";
        std::cout << "  1. Implementation bug in gradient computation\n";
        std::cout << "  2. Theory is incomplete (needs revision)\n";
        std::cout << "  3. Numerical precision issues (unlikely for this magnitude)\n\n";
        
        std::cout << "My opinion: This is likely an IMPLEMENTATION BUG.\n";
        std::cout << "  - The theory is well-established: MAX is monotonically increasing\n";
        std::cout << "  - Negative gradients violate this fundamental property\n";
        std::cout << "  - The gradient accumulation mechanism may have a flaw\n";
    } else {
        std::cout << "✓ All gradients are non-negative (theory satisfied)\n";
    }
    
    return 0;
}

