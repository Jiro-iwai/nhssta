// -*- c++ -*-
// Comprehensive test: Multi-stage MAX with various scenarios
// Generate CSV output for analysis

#include "../src/random_variable.hpp"
#include "../src/max.hpp"
#include "../src/expression.hpp"
#include "../src/normal.hpp"
#include <iostream>
#include <iomanip>
#include <vector>
#include <cmath>

using ::RandomVariable::Normal;
using ::RandomVariable::MAX;
using RandomVar = ::RandomVariable::RandomVariable;

void test_max_max_c_csv() {
    std::cout << "=== MAX(MAX(A, B), C) Sensitivity Analysis ===\n";
    std::cout << "mu_A,mu_B,mu_C,var_A,var_B,var_C,"
              << "output_mean,output_var,"
              << "grad_mu_A,grad_mu_B,grad_mu_C,sum_grad_mu\n";
    
    // Vary mu_C while keeping A and B fixed
    double mu_A = 8.0;
    double mu_B = 10.0;
    double var_A = 4.0;
    double var_B = 1.0;
    double var_C = 2.0;
    
    for (double mu_C = 5.0; mu_C <= 15.0; mu_C += 0.5) {
        Normal A(mu_A, var_A);
        Normal B(mu_B, var_B);
        Normal C(mu_C, var_C);
        
        auto MaxAB = MAX(A, B);
        auto MaxABC = MAX(MaxAB, C);
        
        zero_all_grad();
        auto mean_expr = MaxABC->mean_expr();
        mean_expr->backward();
        
        double grad_A = A->mean_expr()->gradient();
        double grad_B = B->mean_expr()->gradient();
        double grad_C = C->mean_expr()->gradient();
        double sum = grad_A + grad_B + grad_C;
        
        std::cout << std::fixed << std::setprecision(6)
                  << mu_A << "," << mu_B << "," << mu_C << ","
                  << var_A << "," << var_B << "," << var_C << ","
                  << MaxABC->mean() << "," << MaxABC->variance() << ","
                  << grad_A << "," << grad_B << "," << grad_C << ","
                  << sum << "\n";
    }
}

void test_max_a_max_bc_csv() {
    std::cout << "\n=== MAX(A, MAX(B, C)) Sensitivity Analysis ===\n";
    std::cout << "mu_A,mu_B,mu_C,var_A,var_B,var_C,"
              << "output_mean,output_var,"
              << "grad_mu_A,grad_mu_B,grad_mu_C,sum_grad_mu\n";
    
    // Vary mu_A while keeping B and C fixed
    double mu_B = 8.0;
    double mu_C = 10.0;
    double var_A = 4.0;
    double var_B = 1.0;
    double var_C = 2.0;
    
    for (double mu_A = 5.0; mu_A <= 15.0; mu_A += 0.5) {
        Normal A(mu_A, var_A);
        Normal B(mu_B, var_B);
        Normal C(mu_C, var_C);
        
        auto MaxBC = MAX(B, C);
        auto MaxABC = MAX(A, MaxBC);
        
        zero_all_grad();
        auto mean_expr = MaxABC->mean_expr();
        mean_expr->backward();
        
        double grad_A = A->mean_expr()->gradient();
        double grad_B = B->mean_expr()->gradient();
        double grad_C = C->mean_expr()->gradient();
        double sum = grad_A + grad_B + grad_C;
        
        std::cout << std::fixed << std::setprecision(6)
                  << mu_A << "," << mu_B << "," << mu_C << ","
                  << var_A << "," << var_B << "," << var_C << ","
                  << MaxABC->mean() << "," << MaxABC->variance() << ","
                  << grad_A << "," << grad_B << "," << grad_C << ","
                  << sum << "\n";
    }
}

void test_max_max_max_csv() {
    std::cout << "\n=== MAX(MAX(A, B), MAX(C, D)) Sensitivity Analysis ===\n";
    std::cout << "mu_A,mu_B,mu_C,mu_D,var_A,var_B,var_C,var_D,"
              << "output_mean,output_var,"
              << "grad_mu_A,grad_mu_B,grad_mu_C,grad_mu_D,sum_grad_mu\n";
    
    // Vary mu_D while keeping A, B, C fixed
    double mu_A = 5.0;
    double mu_B = 8.0;
    double mu_C = 10.0;
    double var_A = 4.0;
    double var_B = 1.0;
    double var_C = 2.0;
    double var_D = 3.0;
    
    for (double mu_D = 8.0; mu_D <= 15.0; mu_D += 0.5) {
        Normal A(mu_A, var_A);
        Normal B(mu_B, var_B);
        Normal C(mu_C, var_C);
        Normal D(mu_D, var_D);
        
        auto MaxAB = MAX(A, B);
        auto MaxCD = MAX(C, D);
        auto MaxABCD = MAX(MaxAB, MaxCD);
        
        zero_all_grad();
        auto mean_expr = MaxABCD->mean_expr();
        mean_expr->backward();
        
        double grad_A = A->mean_expr()->gradient();
        double grad_B = B->mean_expr()->gradient();
        double grad_C = C->mean_expr()->gradient();
        double grad_D = D->mean_expr()->gradient();
        double sum = grad_A + grad_B + grad_C + grad_D;
        
        std::cout << std::fixed << std::setprecision(6)
                  << mu_A << "," << mu_B << "," << mu_C << "," << mu_D << ","
                  << var_A << "," << var_B << "," << var_C << "," << var_D << ","
                  << MaxABCD->mean() << "," << MaxABCD->variance() << ","
                  << grad_A << "," << grad_B << "," << grad_C << "," << grad_D << ","
                  << sum << "\n";
    }
}

int main(int argc, char* argv[]) {
    if (argc > 1) {
        int test_num = std::atoi(argv[1]);
        switch (test_num) {
            case 1: test_max_max_c_csv(); break;
            case 2: test_max_a_max_bc_csv(); break;
            case 3: test_max_max_max_csv(); break;
            default:
                std::cerr << "Invalid test number. Use 1-3.\n";
                return 1;
        }
    } else {
        // Run all tests
        test_max_max_c_csv();
        test_max_a_max_bc_csv();
        test_max_max_max_csv();
    }
    
    return 0;
}

