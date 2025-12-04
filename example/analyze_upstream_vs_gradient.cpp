// -*- c++ -*-
// Analyze the difference between upstream gradient and MAX0's gradient w.r.t. input

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
    std::cout << "=== Analyze Upstream vs Gradient ===\n\n";
    
    std::cout << "Key distinction:\n";
    std::cout << "  upstream: gradient FLOWING INTO a node (from output side)\n";
    std::cout << "  grad[0]: gradient OF the node w.r.t. its INPUT\n\n";
    
    Normal A(10.0, 4.0);
    Normal B(8.0, 1.0);
    Normal C(12.0, 2.0);
    
    auto MaxAB = MAX(A, B);
    auto MaxAC = MAX(A, C);
    auto MaxABC = MAX(MaxAB, MaxAC);
    
    std::cout << "Expression: MAX(MAX(A,B), MAX(A,C))\n";
    std::cout << "  = MAX(A,C) + MAX0(MAX(A,B) - MAX(A,C))\n";
    std::cout << "  = MAX(A,C) + MAX0(diff) where diff < 0\n\n";
    
    std::cout << "Gradient flow:\n";
    std::cout << "  1. Start from output: ∂L/∂MAX(MAX(A,B), MAX(A,C)) = 1\n";
    std::cout << "  2. Flow to MAX(A,C): upstream = 1 (positive)\n";
    std::cout << "  3. Flow to MAX0(diff): upstream = ?\n\n";
    
    std::cout << "For MAX0(diff):\n";
    std::cout << "  upstream: gradient FLOWING INTO MAX0 from output\n";
    std::cout << "           = ∂MAX(MAX(A,B), MAX(A,C))/∂MAX0(diff)\n";
    std::cout << "           = 1 (since MAX = MAX(A,C) + MAX0(diff))\n\n";
    
    std::cout << "  grad[0]: gradient OF MAX0 w.r.t. its input (diff)\n";
    std::cout << "         = ∂MAX0/∂diff\n";
    std::cout << "         = Φ(diff/σ_diff) ∈ [0, 1] (should be positive!)\n\n";
    
    std::cout << "  contrib: upstream × grad[0]\n";
    std::cout << "         = 1 × (∂MAX0/∂diff)\n";
    std::cout << "         = ∂MAX0/∂diff (should be positive!)\n\n";
    
    std::cout << "BUT: In debug log, we see:\n";
    std::cout << "  upstream = -11.199 (NEGATIVE!)\n";
    std::cout << "  grad[0] = 0.792892 (positive)\n";
    std::cout << "  contrib = -11.199 × 0.792892 = -8.87958 (NEGATIVE!)\n\n";
    
    std::cout << "=== Question: Why is upstream negative? ===\n\n";
    
    std::cout << "The upstream gradient flows FROM the output TO MAX0.\n";
    std::cout << "If MAX = MAX(A,C) + MAX0(diff), then:\n";
    std::cout << "  ∂MAX/∂MAX0(diff) = 1 (positive!)\n\n";
    
    std::cout << "However, the actual expression tree might be more complex:\n";
    std::cout << "  MAX(MAX(A,B), MAX(A,C)) might normalize differently\n";
    std::cout << "  The gradient might flow through multiple paths\n";
    std::cout << "  Negative contributions from other paths might accumulate\n\n";
    
    std::cout << "=== Analysis ===\n\n";
    std::cout << "The negative upstream suggests:\n";
    std::cout << "  1. The gradient path is more complex than MAX = MAX(A,C) + MAX0(diff)\n";
    std::cout << "  2. There are negative contributions from other parts of the expression tree\n";
    std::cout << "  3. The normalization in MAX might affect the gradient flow\n\n";
    
    std::cout << "To answer the question:\n";
    std::cout << "  - ∂MAX0/∂diff (grad[0]) = 0.792892 (POSITIVE - correct!)\n";
    std::cout << "  - upstream = -11.199 (NEGATIVE - this is the problem!)\n";
    std::cout << "  - Therefore: The problem is the upstream gradient, not MAX0's gradient\n";
    
    return 0;
}

