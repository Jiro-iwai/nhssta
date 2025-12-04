// -*- c++ -*-
// Test: Multi-stage MAX operation sensitivity analysis
// Verify correctness of sensitivity analysis for cascaded MAX operations
// e.g., MAX(MAX(A, B), C), MAX(A, MAX(B, C)), etc.

#include "../src/random_variable.hpp"
#include "../src/max.hpp"
#include "../src/expression.hpp"
#include "../src/normal.hpp"
#include <iostream>
#include <iomanip>
#include <vector>
#include <cmath>
#include <cassert>

using ::RandomVariable::Normal;
using ::RandomVariable::MAX;
using RandomVar = ::RandomVariable::RandomVariable;

struct MultiStageResult {
    double output_mean;
    double output_var;
    std::vector<double> grad_mu;
    std::vector<double> grad_var;
    std::vector<double> grad_sigma;
};

// Test case: MAX(MAX(A, B), C)
MultiStageResult test_max_max_c(const std::vector<double>& means, 
                                 const std::vector<double>& vars) {
    assert(means.size() == 3 && vars.size() == 3);
    
    Normal A(means[0], vars[0]);
    Normal B(means[1], vars[1]);
    Normal C(means[2], vars[2]);
    
    auto MaxAB = MAX(A, B);
    auto MaxABC = MAX(MaxAB, C);
    
    MultiStageResult result;
    result.output_mean = MaxABC->mean();
    result.output_var = MaxABC->variance();
    
    // Compute mean gradients separately
    zero_all_grad();
    auto mean_expr = MaxABC->mean_expr();
    mean_expr->backward();
    result.grad_mu = {A->mean_expr()->gradient(), 
                      B->mean_expr()->gradient(), 
                      C->mean_expr()->gradient()};
    
    // Compute variance gradients separately
    zero_all_grad();
    auto var_expr = MaxABC->var_expr();
    var_expr->backward();
    result.grad_var = {A->var_expr()->gradient(),
                       B->var_expr()->gradient(),
                       C->var_expr()->gradient()};
    
    for (size_t i = 0; i < 3; ++i) {
        double sigma = sqrt(vars[i]);
        result.grad_sigma.push_back(2.0 * sigma * result.grad_var[i]);
    }
    
    return result;
}

// Test case: MAX(A, MAX(B, C))
MultiStageResult test_max_a_max_bc(const std::vector<double>& means,
                                    const std::vector<double>& vars) {
    assert(means.size() == 3 && vars.size() == 3);
    
    Normal A(means[0], vars[0]);
    Normal B(means[1], vars[1]);
    Normal C(means[2], vars[2]);
    
    auto MaxBC = MAX(B, C);
    auto MaxABC = MAX(A, MaxBC);
    
    MultiStageResult result;
    result.output_mean = MaxABC->mean();
    result.output_var = MaxABC->variance();
    
    // Compute mean gradients separately
    zero_all_grad();
    auto mean_expr = MaxABC->mean_expr();
    mean_expr->backward();
    result.grad_mu = {A->mean_expr()->gradient(),
                      B->mean_expr()->gradient(),
                      C->mean_expr()->gradient()};
    
    // Compute variance gradients separately
    zero_all_grad();
    auto var_expr = MaxABC->var_expr();
    var_expr->backward();
    result.grad_var = {A->var_expr()->gradient(),
                       B->var_expr()->gradient(),
                       C->var_expr()->gradient()};
    
    for (size_t i = 0; i < 3; ++i) {
        double sigma = sqrt(vars[i]);
        result.grad_sigma.push_back(2.0 * sigma * result.grad_var[i]);
    }
    
    return result;
}

// Test case: MAX(MAX(A, B), MAX(C, D))
MultiStageResult test_max_max_max(const std::vector<double>& means,
                                   const std::vector<double>& vars) {
    assert(means.size() == 4 && vars.size() == 4);
    
    Normal A(means[0], vars[0]);
    Normal B(means[1], vars[1]);
    Normal C(means[2], vars[2]);
    Normal D(means[3], vars[3]);
    
    auto MaxAB = MAX(A, B);
    auto MaxCD = MAX(C, D);
    auto MaxABCD = MAX(MaxAB, MaxCD);
    
    MultiStageResult result;
    result.output_mean = MaxABCD->mean();
    result.output_var = MaxABCD->variance();
    
    // Compute mean gradients separately
    zero_all_grad();
    auto mean_expr = MaxABCD->mean_expr();
    mean_expr->backward();
    result.grad_mu = {A->mean_expr()->gradient(),
                      B->mean_expr()->gradient(),
                      C->mean_expr()->gradient(),
                      D->mean_expr()->gradient()};
    
    // Compute variance gradients separately
    zero_all_grad();
    auto var_expr = MaxABCD->var_expr();
    var_expr->backward();
    result.grad_var = {A->var_expr()->gradient(),
                       B->var_expr()->gradient(),
                       C->var_expr()->gradient(),
                       D->var_expr()->gradient()};
    
    for (size_t i = 0; i < 4; ++i) {
        double sigma = sqrt(vars[i]);
        result.grad_sigma.push_back(2.0 * sigma * result.grad_var[i]);
    }
    
    return result;
}

void print_result(const MultiStageResult& result, const std::string& name) {
    std::cout << std::fixed << std::setprecision(6);
    std::cout << "\n=== " << name << " ===\n";
    std::cout << "Output mean: " << result.output_mean << "\n";
    std::cout << "Output var: " << result.output_var << "\n";
    
    double sum_mu = 0.0;
    std::cout << "Gradients w.r.t. mean:\n";
    for (size_t i = 0; i < result.grad_mu.size(); ++i) {
        std::cout << "  grad_mu[" << i << "] = " << result.grad_mu[i] << "\n";
        sum_mu += result.grad_mu[i];
        
        // Check range
        if (result.grad_mu[i] < -0.01 || result.grad_mu[i] > 1.01) {
            std::cout << "    ⚠️  Out of range [0, 1]!\n";
        }
    }
    std::cout << "  Sum = " << sum_mu << " (should be 1.0)\n";
    
    if (std::abs(sum_mu - 1.0) > 1e-6) {
        std::cout << "  ⚠️  FAILED: Sum != 1.0\n";
    } else {
        std::cout << "  ✓ PASSED: Sum = 1.0\n";
    }
}

bool verify_result(const MultiStageResult& result, const std::string& name) {
    bool passed = true;
    
    // Check: Sum of gradients = 1.0
    double sum_mu = 0.0;
    for (double g : result.grad_mu) {
        sum_mu += g;
    }
    
    if (std::abs(sum_mu - 1.0) > 1e-6) {
        std::cout << "⚠️  " << name << ": Sum of gradients = " << sum_mu 
                  << " != 1.0\n";
        passed = false;
    }
    
    // Check: Each gradient in [0, 1]
    for (size_t i = 0; i < result.grad_mu.size(); ++i) {
        if (result.grad_mu[i] < -0.01 || result.grad_mu[i] > 1.01) {
            std::cout << "⚠️  " << name << ": grad_mu[" << i << "] = " 
                      << result.grad_mu[i] << " out of range [0, 1]\n";
            passed = false;
        }
    }
    
    // Check: Output variance is non-negative
    if (result.output_var < -1e-10) {
        std::cout << "⚠️  " << name << ": Negative output variance = " 
                  << result.output_var << "\n";
        passed = false;
    }
    
    return passed;
}

int main() {
    std::cout << std::fixed << std::setprecision(6);
    std::cout << "=== Multi-Stage MAX Operation Sensitivity Analysis ===\n";
    std::cout << "Verifying correctness of cascaded MAX operations\n\n";
    
    bool all_passed = true;
    
    // Test 1: MAX(MAX(A, B), C) where A < B < C
    std::cout << "Test 1: MAX(MAX(A, B), C) where mu_A < mu_B < mu_C\n";
    std::vector<double> means1 = {5.0, 8.0, 12.0};
    std::vector<double> vars1 = {4.0, 1.0, 2.0};
    auto result1 = test_max_max_c(means1, vars1);
    print_result(result1, "MAX(MAX(A, B), C)");
    if (!verify_result(result1, "Test 1")) {
        all_passed = false;
    }
    
    // Test 2: MAX(MAX(A, B), C) where A > B > C
    std::cout << "\nTest 2: MAX(MAX(A, B), C) where mu_A > mu_B > mu_C\n";
    std::vector<double> means2 = {12.0, 8.0, 5.0};
    std::vector<double> vars2 = {2.0, 1.0, 4.0};
    auto result2 = test_max_max_c(means2, vars2);
    print_result(result2, "MAX(MAX(A, B), C)");
    if (!verify_result(result2, "Test 2")) {
        all_passed = false;
    }
    
    // Test 3: MAX(A, MAX(B, C)) where A < B < C
    std::cout << "\nTest 3: MAX(A, MAX(B, C)) where mu_A < mu_B < mu_C\n";
    std::vector<double> means3 = {5.0, 8.0, 12.0};
    std::vector<double> vars3 = {4.0, 1.0, 2.0};
    auto result3 = test_max_a_max_bc(means3, vars3);
    print_result(result3, "MAX(A, MAX(B, C))");
    if (!verify_result(result3, "Test 3")) {
        all_passed = false;
    }
    
    // Test 4: MAX(A, MAX(B, C)) where A > B > C
    std::cout << "\nTest 4: MAX(A, MAX(B, C)) where mu_A > mu_B > mu_C\n";
    std::vector<double> means4 = {12.0, 8.0, 5.0};
    std::vector<double> vars4 = {2.0, 1.0, 4.0};
    auto result4 = test_max_a_max_bc(means4, vars4);
    print_result(result4, "MAX(A, MAX(B, C))");
    if (!verify_result(result4, "Test 4")) {
        all_passed = false;
    }
    
    // Test 5: MAX(MAX(A, B), MAX(C, D)) - 4 inputs
    std::cout << "\nTest 5: MAX(MAX(A, B), MAX(C, D))\n";
    std::vector<double> means5 = {5.0, 8.0, 10.0, 12.0};
    std::vector<double> vars5 = {4.0, 1.0, 2.0, 3.0};
    auto result5 = test_max_max_max(means5, vars5);
    print_result(result5, "MAX(MAX(A, B), MAX(C, D))");
    if (!verify_result(result5, "Test 5")) {
        all_passed = false;
    }
    
    // Test 6: Equal means
    std::cout << "\nTest 6: MAX(MAX(A, B), C) where mu_A = mu_B = mu_C\n";
    std::vector<double> means6 = {10.0, 10.0, 10.0};
    std::vector<double> vars6 = {4.0, 1.0, 2.0};
    auto result6 = test_max_max_c(means6, vars6);
    print_result(result6, "MAX(MAX(A, B), C) - Equal means");
    if (!verify_result(result6, "Test 6")) {
        all_passed = false;
    }
    
    // Test 7: MAX(MAX(A, B), MAX(A, C)) - A is shared
    std::cout << "\nTest 7: MAX(MAX(A, B), MAX(A, C)) - A is shared\n";
    Normal A7(10.0, 4.0);
    Normal B7(8.0, 1.0);
    Normal C7(12.0, 2.0);
    
    auto MaxAB7 = MAX(A7, B7);
    auto MaxAC7 = MAX(A7, C7);
    auto MaxABC7 = MAX(MaxAB7, MaxAC7);
    
    zero_all_grad();
    auto mean_expr7 = MaxABC7->mean_expr();
    mean_expr7->backward();
    
    double grad_A7 = A7->mean_expr()->gradient();
    double grad_B7 = B7->mean_expr()->gradient();
    double grad_C7 = C7->mean_expr()->gradient();
    double sum7 = grad_A7 + grad_B7 + grad_C7;
    
    std::cout << "  grad_mu_A = " << grad_A7 << "\n";
    std::cout << "  grad_mu_B = " << grad_B7 << "\n";
    std::cout << "  grad_mu_C = " << grad_C7 << "\n";
    std::cout << "  Sum = " << sum7 << " (should be 1.0)\n";
    
    if (std::abs(sum7 - 1.0) > 1e-6) {
        std::cout << "  ⚠️  FAILED: Sum != 1.0\n";
        all_passed = false;
    } else {
        std::cout << "  ✓ PASSED: Sum = 1.0\n";
    }
    
    // Summary
    std::cout << "\n=== Summary ===\n";
    if (all_passed && std::abs(sum7 - 1.0) <= 1e-6) {
        std::cout << "✓ All tests PASSED\n";
        std::cout << "Multi-stage MAX operations work correctly!\n";
        return 0;
    } else {
        std::cout << "⚠️  Some tests FAILED\n";
        return 1;
    }
}

