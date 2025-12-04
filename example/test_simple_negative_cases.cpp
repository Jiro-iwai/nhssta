// -*- c++ -*-
// Simple test cases to demonstrate negative gradient conditions

#include "../src/random_variable.hpp"
#include "../src/max.hpp"
#include "../src/expression.hpp"
#include "../src/normal.hpp"
#include <iostream>
#include <iomanip>

using ::RandomVariable::Normal;
using ::RandomVariable::MAX;
using RandomVar = ::RandomVariable::RandomVariable;

void test_case(const std::string& name, double mu_A, double mu_B, double mu_C,
               double var_A, double var_B, double var_C) {
    Normal A(mu_A, var_A);
    Normal B(mu_B, var_B);
    Normal C(mu_C, var_C);
    
    auto MaxAB = MAX(A, B);
    auto MaxAC = MAX(A, C);
    auto MaxABC = MAX(MaxAB, MaxAC);
    
    zero_all_grad();
    auto mean_expr = MaxABC->mean_expr();
    mean_expr->backward();
    
    double grad_A = A->mean_expr()->gradient();
    double grad_B = B->mean_expr()->gradient();
    double grad_C = C->mean_expr()->gradient();
    double sum = grad_A + grad_B + grad_C;
    
    std::cout << std::fixed << std::setprecision(6);
    std::cout << "\n" << name << ":\n";
    std::cout << "  A ~ N(" << mu_A << ", " << var_A << ")\n";
    std::cout << "  B ~ N(" << mu_B << ", " << var_B << ")\n";
    std::cout << "  C ~ N(" << mu_C << ", " << var_C << ")\n";
    std::cout << "  grad_A = " << grad_A << "\n";
    std::cout << "  grad_B = " << grad_B;
    if (grad_B < -1e-10) std::cout << " [NEGATIVE]";
    std::cout << "\n";
    std::cout << "  grad_C = " << grad_C;
    if (grad_C < -1e-10) std::cout << " [NEGATIVE]";
    std::cout << "\n";
    std::cout << "  Sum = " << sum << "\n";
    
    // Analysis
    std::cout << "  Analysis:\n";
    if (mu_B < mu_A && mu_C > mu_A) {
        std::cout << "    - mu_B < mu_A: B loses to A in MAX(A,B)\n";
        std::cout << "    - mu_C > mu_A: C wins in MAX(A,C)\n";
        std::cout << "    - Result: MAX(MAX(A,B), MAX(A,C)) ≈ MAX(A,C)\n";
        std::cout << "    - B's contribution is negative → grad_B < 0\n";
    } else if (mu_C < mu_A && mu_B > mu_A) {
        std::cout << "    - mu_C < mu_A: C loses to A in MAX(A,C)\n";
        std::cout << "    - mu_B > mu_A: B wins in MAX(A,B)\n";
        std::cout << "    - Result: MAX(MAX(A,B), MAX(A,C)) ≈ MAX(A,B)\n";
        std::cout << "    - C's contribution is negative → grad_C < 0\n";
    }
}

int main() {
    std::cout << "=== Simple Test Cases: Negative Gradient Conditions ===\n";
    
    // Case 1: B has negative gradient
    test_case("Case 1: B has negative gradient",
              10.0, 8.0, 12.0,  // mu_A, mu_B, mu_C
              4.0, 1.0, 2.0);   // var_A, var_B, var_C
    
    // Case 2: C has negative gradient
    test_case("Case 2: C has negative gradient",
              10.0, 12.0, 7.0,  // mu_A, mu_B, mu_C (mu_C must be smaller to get negative)
              4.0, 1.0, 2.0);   // var_A, var_B, var_C
    
    // Case 3: No negative gradient (A is maximum)
    test_case("Case 3: No negative gradient (A is maximum)",
              12.0, 8.0, 10.0,  // mu_A, mu_B, mu_C
              4.0, 1.0, 2.0);   // var_A, var_B, var_C
    
    // Case 4: No negative gradient (A is minimum)
    test_case("Case 4: No negative gradient (A is minimum)",
              8.0, 10.0, 12.0,  // mu_A, mu_B, mu_C
              4.0, 1.0, 2.0);   // var_A, var_B, var_C
    
    // Case 5: Extreme case - B much smaller than A
    test_case("Case 5: Extreme case - B much smaller than A",
              10.0, 5.0, 15.0,  // mu_A, mu_B, mu_C
              4.0, 1.0, 2.0);   // var_A, var_B, var_C
    
    std::cout << "\n=== General Rule ===\n";
    std::cout << "Negative gradient occurs when:\n";
    std::cout << "1. Shared input A has intermediate mean\n";
    std::cout << "2. One non-shared input (B or C) has smaller mean than A\n";
    std::cout << "3. The other non-shared input has larger mean than A\n";
    std::cout << "4. The smaller input loses in its local MAX operation\n";
    std::cout << "5. The final result is dominated by the larger input\n";
    
    return 0;
}

