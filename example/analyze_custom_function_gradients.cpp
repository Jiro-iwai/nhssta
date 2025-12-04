// -*- c++ -*-
// Analyze custom function gradients to find why negative gradients occur

#include "../src/random_variable.hpp"
#include "../src/max.hpp"
#include "../src/expression.hpp"
#include "../src/normal.hpp"
#include "../src/statistical_functions.hpp"
#include <iostream>
#include <iomanip>
#include <cmath>

using ::RandomVariable::Normal;
using ::RandomVariable::MAX;
using ::RandomVariable::MAX0;
using RandomVar = ::RandomVariable::RandomVariable;
using RandomVariable::MeanMax_expr;
using RandomVariable::Phi_expr;

int main() {
    std::cout << std::fixed << std::setprecision(8);
    std::cout << "=== Analyze Custom Function Gradients ===\n\n";
    
    // Test 1: MeanMax gradient
    std::cout << "Test 1: MeanMax gradient\n";
    std::cout << "MeanMax(a) = φ(a) + a × Φ(a)\n";
    std::cout << "∂MeanMax/∂a = Φ(a)\n\n";
    
    Variable a_var;
    a_var = -2.0;  // Negative value
    Expression meanmax_expr = MeanMax_expr(a_var);
    
    zero_all_grad();
    meanmax_expr->backward();
    
    double grad_a = a_var->gradient();
    std::cout << "a = " << a_var->value() << "\n";
    std::cout << "MeanMax(a) = " << meanmax_expr->value() << "\n";
    std::cout << "∂MeanMax/∂a = " << grad_a << "\n";
    std::cout << "Expected: Φ(-2.0) = " << (0.5 * (1.0 + erf(-2.0 / sqrt(2.0)))) << "\n\n";
    
    // Test 2: MAX0 gradient w.r.t. mu
    std::cout << "Test 2: MAX0 gradient w.r.t. mu\n";
    std::cout << "E[MAX0(D)] = μ + σ × MeanMax(-μ/σ)\n";
    std::cout << "∂E[MAX0]/∂μ = 1 + σ × (∂MeanMax/∂a) × (∂a/∂μ)\n";
    std::cout << "            = 1 + σ × Φ(a) × (-1/σ)\n";
    std::cout << "            = 1 - Φ(a) = Φ(μ/σ)\n\n";
    
    Normal D(-0.5, 5.0);  // Negative mean
    auto Max0_D = MAX0(D);
    Expression mean_max0 = Max0_D->mean_expr();
    
    zero_all_grad();
    mean_max0->backward();
    
    double grad_mu = D->mean_expr()->gradient();
    double grad_sigma = D->std_expr()->gradient();
    
    std::cout << "D ~ N(" << D->mean() << ", " << D->variance() << ")\n";
    std::cout << "E[MAX0(D)] = " << Max0_D->mean() << "\n";
    std::cout << "∂E[MAX0]/∂μ = " << grad_mu << "\n";
    std::cout << "∂E[MAX0]/∂σ = " << grad_sigma << "\n";
    
    double a = -D->mean() / sqrt(D->variance());
    double expected_grad_mu = 0.5 * (1.0 + erf(D->mean() / sqrt(D->variance())));
    std::cout << "Expected ∂E[MAX0]/∂μ = Φ(μ/σ) = " << expected_grad_mu << "\n";
    std::cout << "Difference: " << (grad_mu - expected_grad_mu) << "\n\n";
    
    // Test 3: MAX0 gradient w.r.t. sigma (through variance)
    std::cout << "Test 3: MAX0 gradient w.r.t. sigma (through variance)\n";
    std::cout << "σ = sqrt(variance)\n";
    std::cout << "∂E[MAX0]/∂σ = σ × (∂MeanMax/∂a) × (∂a/∂σ)\n";
    std::cout << "            = σ × Φ(a) × (μ/σ²)\n";
    std::cout << "            = μ × Φ(a) / σ\n\n";
    
    std::cout << "But wait, sigma comes from variance:\n";
    std::cout << "σ = sqrt(variance)\n";
    std::cout << "∂σ/∂variance = 1/(2×sqrt(variance)) = 1/(2σ)\n";
    std::cout << "So: ∂E[MAX0]/∂variance = (∂E[MAX0]/∂σ) × (∂σ/∂variance)\n";
    std::cout << "                      = (μ × Φ(a) / σ) × (1/(2σ))\n";
    std::cout << "                      = μ × Φ(a) / (2σ²)\n\n";
    
    // Test 4: MAX(MAX(A, B), MAX(A, C)) - trace B's gradient
    std::cout << "Test 4: MAX(MAX(A, B), MAX(A, C)) - trace B's gradient\n";
    Normal A(10.0, 4.0);
    Normal B(8.0, 1.0);
    Normal C(12.0, 2.0);
    
    auto MaxAB = MAX(A, B);
    auto MaxAC = MAX(A, C);
    auto MaxABC = MAX(MaxAB, MaxAC);
    
    std::cout << "A ~ N(10.0, 4.0), B ~ N(8.0, 1.0), C ~ N(12.0, 2.0)\n";
    std::cout << "E[MaxAB] = " << MaxAB->mean() << "\n";
    std::cout << "E[MaxAC] = " << MaxAC->mean() << "\n";
    std::cout << "E[MaxABC] = " << MaxABC->mean() << "\n\n";
    
    // Check normalization
    std::cout << "Normalization check:\n";
    std::cout << "  mu_A = " << A->mean() << ", mu_B = " << B->mean() << "\n";
    if (A->mean() >= B->mean()) {
        std::cout << "  MAX(A, B) = A + MAX0(B-A)\n";
    } else {
        std::cout << "  MAX(A, B) = B + MAX0(A-B)\n";
    }
    std::cout << "  mu_A = " << A->mean() << ", mu_C = " << C->mean() << "\n";
    if (A->mean() >= C->mean()) {
        std::cout << "  MAX(A, C) = A + MAX0(C-A)\n";
    } else {
        std::cout << "  MAX(A, C) = C + MAX0(A-C)\n";
    }
    std::cout << "\n";
    
    zero_all_grad();
    Expression mean_expr = MaxABC->mean_expr();
    mean_expr->backward();
    
    double grad_A = A->mean_expr()->gradient();
    double grad_B = B->mean_expr()->gradient();
    double grad_C = C->mean_expr()->gradient();
    
    std::cout << "Final gradients:\n";
    std::cout << "  grad_A = " << grad_A << "\n";
    std::cout << "  grad_B = " << grad_B;
    if (grad_B < -1e-10) {
        std::cout << " [NEGATIVE!]";
    }
    std::cout << "\n";
    std::cout << "  grad_C = " << grad_C << "\n";
    std::cout << "  Sum = " << (grad_A + grad_B + grad_C) << "\n\n";
    
    // Check B's variance gradient
    double grad_var_B = B->var_expr()->gradient();
    std::cout << "B's variance gradient: " << grad_var_B << "\n";
    std::cout << "B's std gradient: " << B->std_expr()->gradient() << "\n";
    std::cout << "B's mean gradient: " << grad_B << "\n\n";
    
    // The key insight: sigma depends on variance
    // If variance gradient is negative, and sigma = sqrt(variance),
    // then the gradient through sigma can be negative
    
    return 0;
}

