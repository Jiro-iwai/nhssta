// -*- c++ -*-
// Analyze MAX expression tree structure to understand upstream gradient

#include "../src/random_variable.hpp"
#include "../src/max.hpp"
#include "../src/expression.hpp"
#include "../src/normal.hpp"
#include <iostream>
#include <iomanip>

using ::RandomVariable::Normal;
using ::RandomVariable::MAX;
using RandomVar = ::RandomVariable::RandomVariable;

int main() {
    std::cout << std::fixed << std::setprecision(8);
    std::cout << "=== Analyze MAX Expression Tree Structure ===\n\n";
    
    Normal A(10.0, 4.0);
    Normal B(8.0, 1.0);
    Normal C(12.0, 2.0);
    
    std::cout << "Expression: MAX(MAX(A,B), MAX(A,C))\n\n";
    
    auto MaxAB = MAX(A, B);
    auto MaxAC = MAX(A, C);
    auto MaxABC = MAX(MaxAB, MaxAC);
    
    std::cout << "MAX(A,B) mean expression structure:\n";
    std::cout << "  MAX(A,B) = A + MAX0(B-A)\n";
    std::cout << "  mean_expr = A.mean_expr() + MAX0(B-A).mean_expr()\n";
    std::cout << "            = A.mean_expr() + MAX0_mean(B-A.mean_expr(), B-A.std_expr())\n\n";
    
    std::cout << "MAX(A,C) mean expression structure:\n";
    std::cout << "  MAX(A,C) = C + MAX0(A-C)\n";
    std::cout << "  mean_expr = C.mean_expr() + MAX0(A-C).mean_expr()\n";
    std::cout << "            = C.mean_expr() + MAX0_mean(A-C.mean_expr(), A-C.std_expr())\n\n";
    
    std::cout << "MAX(MAX(A,B), MAX(A,C)) mean expression structure:\n";
    std::cout << "  MAX(MAX(A,B), MAX(A,C)) = MAX(A,C) + MAX0(MAX(A,B) - MAX(A,C))\n";
    std::cout << "  mean_expr = MAX(A,C).mean_expr() + MAX0(MAX(A,B) - MAX(A,C)).mean_expr()\n";
    std::cout << "            = [C + MAX0(A-C)].mean_expr() + MAX0([A + MAX0(B-A)] - [C + MAX0(A-C)]).mean_expr()\n\n";
    
    std::cout << "=== Gradient Flow ===\n\n";
    std::cout << "When computing ∂MAX/∂MAX0(MAX(A,B) - MAX(A,C)):\n";
    std::cout << "  The gradient flows through:\n";
    std::cout << "    1. MAX = MAX(A,C) + MAX0(diff)\n";
    std::cout << "    2. ∂MAX/∂MAX0(diff) = 1 (direct path - positive)\n";
    std::cout << "    3. But also through: MAX(A,B) - MAX(A,C)\n";
    std::cout << "    4. The MINUS operation: diff = MAX(A,B) - MAX(A,C)\n";
    std::cout << "    5. ∂diff/∂MAX(A,B) = 1 (positive)\n";
    std::cout << "    6. ∂diff/∂MAX(A,C) = -1 (negative!)\n\n";
    
    std::cout << "The problem:\n";
    std::cout << "  MAX0(diff) depends on diff = MAX(A,B) - MAX(A,C)\n";
    std::cout << "  When computing gradient w.r.t. MAX(A,C):\n";
    std::cout << "    ∂MAX0(diff)/∂MAX(A,C) = (∂MAX0/∂diff) × (∂diff/∂MAX(A,C))\n";
    std::cout << "                          = (∂MAX0/∂diff) × (-1)\n";
    std::cout << "                          = negative!\n\n";
    
    std::cout << "But MAX(A,C) is also part of the output:\n";
    std::cout << "  MAX = MAX(A,C) + MAX0(diff)\n";
    std::cout << "  ∂MAX/∂MAX(A,C) = 1 + ∂MAX0(diff)/∂MAX(A,C)\n";
    std::cout << "                 = 1 + (negative value)\n";
    std::cout << "  This can become negative if |∂MAX0(diff)/∂MAX(A,C)| > 1\n\n";
    
    std::cout << "Then, when this negative gradient flows to MAX0(diff):\n";
    std::cout << "  upstream = ∂MAX/∂MAX0(diff)\n";
    std::cout << "  But MAX depends on MAX(A,C), which has negative gradient\n";
    std::cout << "  So upstream accumulates negative contributions!\n";
    
    return 0;
}

