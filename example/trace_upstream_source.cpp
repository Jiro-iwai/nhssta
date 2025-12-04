// -*- c++ -*-
// Trace why upstream becomes negative

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
    std::cout << "=== Trace Why Upstream Becomes Negative ===\n\n";
    
    Normal A(10.0, 4.0);
    Normal B(8.0, 1.0);
    Normal C(12.0, 2.0);
    
    std::cout << "Expression: MAX(MAX(A,B), MAX(A,C))\n";
    std::cout << "  A ~ N(" << A->mean() << ", " << A->variance() << ")\n";
    std::cout << "  B ~ N(" << B->mean() << ", " << B->variance() << ")\n";
    std::cout << "  C ~ N(" << C->mean() << ", " << C->variance() << ")\n\n";
    
    auto MaxAB = MAX(A, B);
    auto MaxAC = MAX(A, C);
    auto MaxABC = MAX(MaxAB, MaxAC);
    
    std::cout << "Step 1: MAX(A,B) = A + MAX0(B-A)\n";
    std::cout << "  E[MAX(A,B)] = " << MaxAB->mean() << "\n";
    std::cout << "  Since mu_A > mu_B, normalized to: A + MAX0(B-A)\n\n";
    
    std::cout << "Step 2: MAX(A,C) = C + MAX0(A-C)\n";
    std::cout << "  E[MAX(A,C)] = " << MaxAC->mean() << "\n";
    std::cout << "  Since mu_C > mu_A, normalized to: C + MAX0(A-C)\n\n";
    
    std::cout << "Step 3: MAX(MAX(A,B), MAX(A,C))\n";
    std::cout << "  E[MAX(A,B)] = " << MaxAB->mean() << "\n";
    std::cout << "  E[MAX(A,C)] = " << MaxAC->mean() << "\n";
    std::cout << "  Since E[MAX(A,C)] > E[MAX(A,B)]\n";
    std::cout << "  MAX(MAX(A,B), MAX(A,C)) = MAX(A,C) + MAX0(MAX(A,B) - MAX(A,C))\n";
    std::cout << "  = C + MAX0(A-C) + MAX0(MAX(A,B) - MAX(A,C))\n\n";
    
    std::cout << "=== Gradient Flow Analysis ===\n\n";
    
    std::cout << "For MAX0(MAX(A,B) - MAX(A,C)):\n";
    std::cout << "  upstream = ∂MAX(MAX(A,B), MAX(A,C))/∂MAX0(MAX(A,B) - MAX(A,C))\n";
    std::cout << "  If MAX = MAX(A,C) + MAX0(diff), then:\n";
    std::cout << "    upstream = ∂(MAX(A,C) + MAX0(diff))/∂MAX0(diff)\n";
    std::cout << "            = 1 (positive!)\n\n";
    
    std::cout << "BUT: The actual expression tree is more complex!\n";
    std::cout << "  MAX(A,C) = C + MAX0(A-C)\n";
    std::cout << "  So: MAX = C + MAX0(A-C) + MAX0(MAX(A,B) - MAX(A,C))\n\n";
    
    std::cout << "The gradient flows through:\n";
    std::cout << "  1. Output → MAX(A,C) → C (positive)\n";
    std::cout << "  2. Output → MAX(A,C) → MAX0(A-C) (positive)\n";
    std::cout << "  3. Output → MAX0(MAX(A,B) - MAX(A,C)) (should be positive)\n\n";
    
    std::cout << "However, MAX0(MAX(A,B) - MAX(A,C)) receives upstream from:\n";
    std::cout << "  - Direct path: Output → MAX0(diff) = 1\n";
    std::cout << "  - But also through: MAX(A,B) - MAX(A,C) computation\n";
    std::cout << "  - The MINUS operation: MAX(A,B) - MAX(A,C)\n";
    std::cout << "    This MINUS has: ∂(left-right)/∂right = -1\n";
    std::cout << "  - So the gradient through MINUS is negative!\n\n";
    
    std::cout << "=== Key Insight ===\n\n";
    std::cout << "The upstream gradient accumulates from multiple paths:\n";
    std::cout << "  1. Direct path: +1 (positive)\n";
    std::cout << "  2. Through MINUS: negative contributions\n";
    std::cout << "  3. Through other intermediate nodes: possibly negative\n";
    std::cout << "  Result: upstream = -11.199 (negative!)\n\n";
    
    std::cout << "The problem is that the gradient accumulates through the\n";
    std::cout << "expression tree structure, and negative contributions from\n";
    std::cout << "MINUS operations accumulate to make upstream negative.\n";
    
    return 0;
}

