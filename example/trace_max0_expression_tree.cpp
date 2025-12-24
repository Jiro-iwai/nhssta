// -*- c++ -*-
// Trace MAX0 expression tree to understand negative gradient path

#include "../src/random_variable.hpp"
#include "../src/max.hpp"
#include "../src/expression.hpp"
#include "../src/normal.hpp"
#include "../src/statistical_functions.hpp"
#include <iostream>
#include <iomanip>

using ::RandomVariable::Normal;
using ::RandomVariable::MAX;
using RandomVar = ::RandomVariable::RandomVariable;
using RandomVariable::max0_mean_expr;

int main() {
    std::cout << std::fixed << std::setprecision(8);
    std::cout << "=== Trace MAX0 Expression Tree ===\n\n";
    
    // MAX0 formula: E[max(0, D)] = μ + σ × MeanMax(-μ/σ)
    // Let's build this step by step
    
    Variable mu_var, sigma_var;
    mu_var = -2.0;   // Like B-A when mu_B < mu_A
    sigma_var = sqrt(5.0);  // sqrt(var_A + var_B)
    
    std::cout << "Step 1: Build MAX0 expression tree\n";
    std::cout << "  μ = " << mu_var->value() << "\n";
    std::cout << "  σ = " << sigma_var->value() << "\n\n";
    
    // Build a = -(μ/σ) step by step
    Expression mu_div_sigma = mu_var / sigma_var;
    Expression neg_mu_div_sigma = -mu_div_sigma;
    Expression a = neg_mu_div_sigma;
    
    std::cout << "Step 2: Compute a = -(μ/σ)\n";
    std::cout << "  μ/σ = " << mu_div_sigma->value() << "\n";
    std::cout << "  -(μ/σ) = " << neg_mu_div_sigma->value() << "\n";
    std::cout << "  a = " << a->value() << "\n\n";
    
    // Check gradient of a w.r.t. mu
    zero_all_grad();
    a->backward();
    double grad_a_mu = mu_var->gradient();
    double grad_a_sigma = sigma_var->gradient();
    
    std::cout << "Gradients of a:\n";
    std::cout << "  ∂a/∂μ = " << grad_a_mu << "\n";
    std::cout << "  ∂a/∂σ = " << grad_a_sigma << "\n";
    std::cout << "  Expected ∂a/∂μ = -1/σ = " << (-1.0 / sigma_var->value()) << "\n";
    std::cout << "  Expected ∂a/∂σ = μ/σ² = " << (mu_var->value() / (sigma_var->value() * sigma_var->value())) << "\n\n";
    
    // Now build MeanMax(a)
    Expression meanmax_a = MeanMax_expr(a);
    std::cout << "Step 3: Compute MeanMax(a)\n";
    std::cout << "  MeanMax(a) = " << meanmax_a->value() << "\n\n";
    
    // Compute gradient of MeanMax(a) w.r.t. mu
    zero_all_grad();
    meanmax_a->backward();
    double grad_meanmax_mu = mu_var->gradient();
    double grad_meanmax_sigma = sigma_var->gradient();
    
    std::cout << "Gradients of MeanMax(a):\n";
    std::cout << "  ∂MeanMax/∂μ = " << grad_meanmax_mu << "\n";
    std::cout << "  ∂MeanMax/∂σ = " << grad_meanmax_sigma << "\n";
    
    // Expected: ∂MeanMax/∂a = Φ(a)
    double Phi_a = 0.5 * (1.0 + erf(a->value() / sqrt(2.0)));
    double expected_grad_meanmax_mu = Phi_a * (-1.0 / sigma_var->value());
    double expected_grad_meanmax_sigma = Phi_a * (mu_var->value() / (sigma_var->value() * sigma_var->value()));
    
    std::cout << "  Expected ∂MeanMax/∂μ = Φ(a) × (-1/σ) = " << expected_grad_meanmax_mu << "\n";
    std::cout << "  Expected ∂MeanMax/∂σ = Φ(a) × (μ/σ²) = " << expected_grad_meanmax_sigma << "\n";
    std::cout << "  Difference (μ): " << (grad_meanmax_mu - expected_grad_meanmax_mu) << "\n";
    std::cout << "  Difference (σ): " << (grad_meanmax_sigma - expected_grad_meanmax_sigma) << "\n\n";
    
    // Now build MAX0: μ + σ × MeanMax(a)
    Expression max0_expr = mu_var + sigma_var * meanmax_a;
    std::cout << "Step 4: Compute MAX0 = μ + σ × MeanMax(a)\n";
    std::cout << "  MAX0 = " << max0_expr->value() << "\n\n";
    
    zero_all_grad();
    max0_expr->backward();
    double grad_max0_mu = mu_var->gradient();
    double grad_max0_sigma = sigma_var->gradient();
    
    std::cout << "Final gradients:\n";
    std::cout << "  ∂MAX0/∂μ = " << grad_max0_mu << "\n";
    std::cout << "  ∂MAX0/∂σ = " << grad_max0_sigma << "\n";
    
    // Expected: ∂MAX0/∂μ = 1 + σ × (∂MeanMax/∂μ)
    double expected_grad_max0_mu = 1.0 + sigma_var->value() * expected_grad_meanmax_mu;
    double expected_grad_max0_sigma = meanmax_a->value() + sigma_var->value() * expected_grad_meanmax_sigma;
    
    std::cout << "  Expected ∂MAX0/∂μ = 1 + σ × (∂MeanMax/∂μ) = " << expected_grad_max0_mu << "\n";
    std::cout << "  Expected ∂MAX0/∂σ = MeanMax(a) + σ × (∂MeanMax/∂σ) = " << expected_grad_max0_sigma << "\n";
    std::cout << "  Difference (μ): " << (grad_max0_mu - expected_grad_max0_mu) << "\n";
    std::cout << "  Difference (σ): " << (grad_max0_sigma - expected_grad_max0_sigma) << "\n\n";
    
    // The key question: Can grad_max0_sigma be negative?
    // If sigma = sqrt(variance), and variance gradient is negative,
    // then sigma gradient can be negative
    
    return 0;
}

