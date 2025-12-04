// -*- c++ -*-
// Experiment: MAX operation sensitivity analysis
// This program analyzes how output delay sensitivity changes when
// input delay mean and variance are varied.

#include <iostream>
#include <iomanip>
#include <vector>
#include <cmath>

#include "../src/random_variable.hpp"
#include "../src/max.hpp"
#include "../src/expression.hpp"
#include "../src/normal.hpp"

namespace {
using RandomVariable::Normal;
using RandomVariable::MAX;
using RandomVar = RandomVariable::RandomVariable;
}

// Helper function to compute sensitivity gradients
struct SensitivityResults {
    double output_mean;
    double output_var;
    double grad_mu_A;      // ∂E[MAX]/∂μ_A
    double grad_var_A;      // ∂Var[MAX]/∂σ²_A
    double grad_mu_B;       // ∂E[MAX]/∂μ_B
    double grad_var_B;      // ∂Var[MAX]/∂σ²_B
};

SensitivityResults compute_sensitivity(const RandomVar& A, const RandomVar& B) {
    auto MaxAB = MAX(A, B);
    
    SensitivityResults results;
    results.output_mean = MaxAB->mean();
    results.output_var = MaxAB->variance();
    
    // Compute mean expression gradients
    zero_all_grad();
    Expression mean_expr = MaxAB->mean_expr();
    mean_expr->backward();
    results.grad_mu_A = A->mean_expr()->gradient();
    results.grad_mu_B = B->mean_expr()->gradient();
    
    // Compute variance expression gradients (separately to avoid overwriting mean gradients)
    zero_all_grad();
    Expression var_expr = MaxAB->var_expr();
    var_expr->backward();
    results.grad_var_A = A->var_expr()->gradient();
    results.grad_var_B = B->var_expr()->gradient();
    
    return results;
}

// Print CSV header
void print_csv_header() {
    std::cout << "mu_A,var_A,mu_B,var_B,"
              << "output_mean,output_var,"
              << "grad_mu_A,grad_var_A,grad_mu_B,grad_var_B,"
              << "grad_sigma_A,grad_sigma_B" << std::endl;
}

// Print results in CSV format
void print_csv_row(double mu_A, double var_A, double mu_B, double var_B,
                   const SensitivityResults& results) {
    double sigma_A = std::sqrt(var_A);
    double sigma_B = std::sqrt(var_B);
    
    // Convert variance gradient to standard deviation gradient
    // ∂Var/∂σ = 2σ · ∂Var/∂σ²
    double grad_sigma_A = 2.0 * sigma_A * results.grad_var_A;
    double grad_sigma_B = 2.0 * sigma_B * results.grad_var_B;
    
    std::cout << std::fixed << std::setprecision(6)
              << mu_A << "," << var_A << "," << mu_B << "," << var_B << ","
              << results.output_mean << "," << results.output_var << ","
              << results.grad_mu_A << "," << results.grad_var_A << ","
              << results.grad_mu_B << "," << results.grad_var_B << ","
              << grad_sigma_A << "," << grad_sigma_B << std::endl;
}

// Experiment 1: Vary mean of A while keeping others fixed
void experiment_vary_mean_A() {
    std::cout << "\n=== Experiment 1: Vary mean of A ===" << std::endl;
    std::cout << "Fixed: var_A=4.0, mu_B=8.0, var_B=1.0" << std::endl;
    print_csv_header();
    
    double var_A = 4.0;
    double mu_B = 8.0;
    double var_B = 1.0;
    
    for (double mu_A = 5.0; mu_A <= 12.0; mu_A += 0.5) {
        Normal A(mu_A, var_A);
        Normal B(mu_B, var_B);
        
        SensitivityResults results = compute_sensitivity(A, B);
        print_csv_row(mu_A, var_A, mu_B, var_B, results);
    }
}

// Experiment 2: Vary variance of A while keeping others fixed
void experiment_vary_var_A() {
    std::cout << "\n=== Experiment 2: Vary variance of A ===" << std::endl;
    std::cout << "Fixed: mu_A=10.0, mu_B=8.0, var_B=1.0" << std::endl;
    print_csv_header();
    
    double mu_A = 10.0;
    double mu_B = 8.0;
    double var_B = 1.0;
    
    for (double var_A = 0.5; var_A <= 10.0; var_A += 0.5) {
        Normal A(mu_A, var_A);
        Normal B(mu_B, var_B);
        
        SensitivityResults results = compute_sensitivity(A, B);
        print_csv_row(mu_A, var_A, mu_B, var_B, results);
    }
}

// Experiment 3: Vary mean of B while keeping others fixed
void experiment_vary_mean_B() {
    std::cout << "\n=== Experiment 3: Vary mean of B ===" << std::endl;
    std::cout << "Fixed: mu_A=10.0, var_A=4.0, var_B=1.0" << std::endl;
    print_csv_header();
    
    double mu_A = 10.0;
    double var_A = 4.0;
    double var_B = 1.0;
    
    for (double mu_B = 5.0; mu_B <= 12.0; mu_B += 0.5) {
        Normal A(mu_A, var_A);
        Normal B(mu_B, var_B);
        
        SensitivityResults results = compute_sensitivity(A, B);
        print_csv_row(mu_A, var_A, mu_B, var_B, results);
    }
}

// Experiment 4: Vary variance of B while keeping others fixed
void experiment_vary_var_B() {
    std::cout << "\n=== Experiment 4: Vary variance of B ===" << std::endl;
    std::cout << "Fixed: mu_A=10.0, var_A=4.0, mu_B=8.0" << std::endl;
    print_csv_header();
    
    double mu_A = 10.0;
    double var_A = 4.0;
    double mu_B = 8.0;
    
    for (double var_B = 0.5; var_B <= 10.0; var_B += 0.5) {
        Normal A(mu_A, var_A);
        Normal B(mu_B, var_B);
        
        SensitivityResults results = compute_sensitivity(A, B);
        print_csv_row(mu_A, var_A, mu_B, var_B, results);
    }
}

// Experiment 5: Equal means, vary both variances
void experiment_equal_means_vary_vars() {
    std::cout << "\n=== Experiment 5: Equal means, vary both variances ===" << std::endl;
    std::cout << "Fixed: mu_A=10.0, mu_B=10.0" << std::endl;
    print_csv_header();
    
    double mu_A = 10.0;
    double mu_B = 10.0;
    
    for (double var_A = 0.5; var_A <= 10.0; var_A += 0.5) {
        for (double var_B = 0.5; var_B <= 10.0; var_B += 0.5) {
            Normal A(mu_A, var_A);
            Normal B(mu_B, var_B);
            
            SensitivityResults results = compute_sensitivity(A, B);
            print_csv_row(mu_A, var_A, mu_B, var_B, results);
        }
    }
}

int main(int argc, char* argv[]) {
    std::cout << "MAX Operation Sensitivity Analysis Experiment" << std::endl;
    std::cout << "=============================================" << std::endl;
    
    if (argc > 1) {
        int exp_num = std::atoi(argv[1]);
        switch (exp_num) {
            case 1: experiment_vary_mean_A(); break;
            case 2: experiment_vary_var_A(); break;
            case 3: experiment_vary_mean_B(); break;
            case 4: experiment_vary_var_B(); break;
            case 5: experiment_equal_means_vary_vars(); break;
            default:
                std::cerr << "Invalid experiment number. Use 1-5." << std::endl;
                return 1;
        }
    } else {
        // Run all experiments
        experiment_vary_mean_A();
        experiment_vary_var_A();
        experiment_vary_mean_B();
        experiment_vary_var_B();
        experiment_equal_means_vary_vars();
    }
    
    return 0;
}

