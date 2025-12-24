// -*- c++ -*-
// Find where negative upstream gradients originate

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
    std::cout << "=== Find Negative Upstream Gradient Source ===\n\n";
    
    Normal A(10.0, 4.0);
    Normal B(8.0, 1.0);
    Normal C(12.0, 2.0);
    
    auto MaxAB = MAX(A, B);
    auto MaxAC = MAX(A, C);
    auto MaxABC = MAX(MaxAB, MaxAC);
    
    std::cout << "Expression: MAX(MAX(A, B), MAX(A, C))\n";
    std::cout << "A ~ N(10.0, 4.0), B ~ N(8.0, 1.0), C ~ N(12.0, 2.0)\n\n";
    
    // Enable debug
    ExpressionImpl::enable_backward_debug(true, "negative_upstream_debug.log");
    
    zero_all_grad();
    Expression mean_expr = MaxABC->mean_expr();
    mean_expr->backward();
    
    double grad_B = B->mean_expr()->gradient();
    std::cout << "grad_B = " << grad_B;
    if (grad_B < -1e-10) {
        std::cout << " [NEGATIVE!]";
    }
    std::cout << "\n\n";
    
    ExpressionImpl::enable_backward_debug(false);
    
    std::cout << "Check negative_upstream_debug.log for detailed trace.\n";
    std::cout << "Look for negative upstream gradients in MINUS, MUL, DIV operations.\n";
    
    return 0;
}

