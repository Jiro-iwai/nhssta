// -*- c++ -*-
// Test backward() debug output for negative gradient investigation

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
    std::cout << "=== Debug Backward() for Negative Gradient Investigation ===\n\n";
    
    // Enable debug output
    ExpressionImpl::enable_backward_debug(true, "backward_debug.log");
    
    // Test case that produces negative gradient
    Normal A(10.0, 4.0);
    Normal B(8.0, 1.0);
    Normal C(12.0, 2.0);
    
    std::cout << "Input:\n";
    std::cout << "  A ~ N(10.0, 4.0)\n";
    std::cout << "  B ~ N(8.0, 1.0)\n";
    std::cout << "  C ~ N(12.0, 2.0)\n\n";
    
    auto MaxAB = MAX(A, B);
    auto MaxAC = MAX(A, C);
    auto MaxABC = MAX(MaxAB, MaxAC);
    
    std::cout << "Computing gradients...\n";
    std::cout << "Debug output will be written to backward_debug.log\n\n";
    
    zero_all_grad();
    auto mean_expr = MaxABC->mean_expr();
    mean_expr->backward();
    
    double grad_A = A->mean_expr()->gradient();
    double grad_B = B->mean_expr()->gradient();
    double grad_C = C->mean_expr()->gradient();
    
    std::cout << "Results:\n";
    std::cout << "  grad_A = " << grad_A << "\n";
    std::cout << "  grad_B = " << grad_B;
    if (grad_B < -1e-10) {
        std::cout << " [NEGATIVE!]";
    }
    std::cout << "\n";
    std::cout << "  grad_C = " << grad_C << "\n";
    std::cout << "  Sum = " << (grad_A + grad_B + grad_C) << "\n\n";
    
    std::cout << "Check backward_debug.log for detailed trace of gradient propagation.\n";
    std::cout << "Look for 'NEGATIVE GRADIENT' warnings to find where negative gradients occur.\n";
    
    // Disable debug output
    ExpressionImpl::enable_backward_debug(false);
    
    return 0;
}

