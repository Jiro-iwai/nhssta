// -*- c++ -*-
// Test: sigma gradient when sigma = sqrt(variance)
// This is the key to understanding negative gradients

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
using RandomVariable::MeanMax_expr;

int main() {
    std::cout << std::fixed << std::setprecision(8);
    std::cout << "=== Test: Sigma Gradient from Variance ===\n\n";
    
    // The key insight: sigma = sqrt(variance)
    // If variance gradient is negative, sigma gradient is negative
    
    Variable var_var;
    var_var = 5.0;  // variance value
    
    Expression sigma_expr = sqrt(var_var);
    
    std::cout << "Test 1: Direct sigma = sqrt(variance)\n";
    std::cout << "  variance = " << var_var->value() << "\n";
    std::cout << "  sigma = sqrt(variance) = " << sigma_expr->value() << "\n\n";
    
    // Compute gradient of sigma w.r.t. variance
    zero_all_grad();
    sigma_expr->backward();
    double grad_sigma_var = var_var->gradient();
    
    std::cout << "Gradient:\n";
    std::cout << "  ∂σ/∂variance = " << grad_sigma_var << "\n";
    std::cout << "  Expected = 1/(2×sqrt(variance)) = " << (1.0 / (2.0 * sqrt(var_var->value()))) << "\n";
    std::cout << "  Difference: " << (grad_sigma_var - (1.0 / (2.0 * sqrt(var_var->value())))) << "\n\n";
    
    // Test 2: What if variance gradient is negative?
    std::cout << "Test 2: Negative variance gradient\n";
    std::cout << "  If ∂f/∂variance = -1.0 (negative)\n";
    std::cout << "  Then: ∂f/∂σ = (∂f/∂variance) × (∂σ/∂variance)\n";
    std::cout << "              = -1.0 × " << (1.0 / (2.0 * sqrt(var_var->value()))) << "\n";
    std::cout << "              = " << (-1.0 / (2.0 * sqrt(var_var->value()))) << " (negative!)\n\n";
    
    // Test 3: MAX0 with sigma from variance
    std::cout << "Test 3: MAX0 with sigma = sqrt(variance)\n";
    Variable mu_var;
    mu_var = -2.0;
    
    Expression max0_expr = mu_var + sigma_expr * MeanMax_expr(-(mu_var / sigma_expr));
    
    zero_all_grad();
    max0_expr->backward();
    
    double grad_max0_mu = mu_var->gradient();
    double grad_max0_var = var_var->gradient();
    
    std::cout << "MAX0 gradients:\n";
    std::cout << "  ∂MAX0/∂μ = " << grad_max0_mu << "\n";
    std::cout << "  ∂MAX0/∂variance = " << grad_max0_var << "\n\n";
    
    // The question: Can grad_max0_var be negative?
    // If so, this could cause negative mean gradient in multi-stage MAX
    
    return 0;
}

