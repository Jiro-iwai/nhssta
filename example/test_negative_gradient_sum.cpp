// -*- c++ -*-
// Test: Verify that gradient sum = 1.0 even when negative gradients occur
// Focus on shared-input cases where negative gradients are more likely

#include "../src/random_variable.hpp"
#include "../src/max.hpp"
#include "../src/expression.hpp"
#include "../src/normal.hpp"
#include <iostream>
#include <iomanip>
#include <vector>
#include <cmath>
#include <iomanip>

using ::RandomVariable::Normal;
using ::RandomVariable::MAX;
using RandomVar = ::RandomVariable::RandomVariable;

struct GradientResult {
    std::vector<double> grad_mu;
    double sum;
    bool has_negative;
    int negative_count;
};

GradientResult test_max_max_shared(const std::vector<double>& means,
                                   const std::vector<double>& vars) {
    assert(means.size() == 3 && vars.size() == 3);
    
    Normal A(means[0], vars[0]);
    Normal B(means[1], vars[1]);
    Normal C(means[2], vars[2]);
    
    // MAX(MAX(A, B), MAX(A, C)) - A is shared
    auto MaxAB = MAX(A, B);
    auto MaxAC = MAX(A, C);
    auto MaxABC = MAX(MaxAB, MaxAC);
    
    zero_all_grad();
    auto mean_expr = MaxABC->mean_expr();
    mean_expr->backward();
    
    GradientResult result;
    result.grad_mu = {A->mean_expr()->gradient(),
                      B->mean_expr()->gradient(),
                      C->mean_expr()->gradient()};
    
    result.sum = 0.0;
    result.has_negative = false;
    result.negative_count = 0;
    
    for (double g : result.grad_mu) {
        result.sum += g;
        if (g < -1e-10) {
            result.has_negative = true;
            result.negative_count++;
        }
    }
    
    return result;
}

GradientResult test_max_max_max_shared(const std::vector<double>& means,
                                        const std::vector<double>& vars) {
    assert(means.size() == 4 && vars.size() == 4);
    
    Normal A(means[0], vars[0]);
    Normal B(means[1], vars[1]);
    Normal C(means[2], vars[2]);
    Normal D(means[3], vars[3]);
    
    // MAX(MAX(A, B), MAX(A, C), MAX(A, D)) - A is shared in all
    // Actually: MAX(MAX(MAX(A, B), MAX(A, C)), MAX(A, D))
    auto MaxAB = MAX(A, B);
    auto MaxAC = MAX(A, C);
    auto MaxABC = MAX(MaxAB, MaxAC);
    auto MaxAD = MAX(A, D);
    auto MaxABCD = MAX(MaxABC, MaxAD);
    
    zero_all_grad();
    auto mean_expr = MaxABCD->mean_expr();
    mean_expr->backward();
    
    GradientResult result;
    result.grad_mu = {A->mean_expr()->gradient(),
                      B->mean_expr()->gradient(),
                      C->mean_expr()->gradient(),
                      D->mean_expr()->gradient()};
    
    result.sum = 0.0;
    result.has_negative = false;
    result.negative_count = 0;
    
    for (double g : result.grad_mu) {
        result.sum += g;
        if (g < -1e-10) {
            result.has_negative = true;
            result.negative_count++;
        }
    }
    
    return result;
}

int main() {
    std::cout << std::fixed << std::setprecision(6);
    std::cout << "=== Negative Gradient Sum Verification ===\n";
    std::cout << "Testing: Does gradient sum = 1.0 even when negative gradients occur?\n\n";
    
    int total_tests = 0;
    int tests_with_negative = 0;
    int tests_sum_not_one = 0;
    double max_sum_error = 0.0;
    double min_sum_error = 1e10;
    
    // Test 1: MAX(MAX(A, B), MAX(A, C)) with various parameters
    std::cout << "Test 1: MAX(MAX(A, B), MAX(A, C)) - A is shared\n";
    std::cout << "Varying mu_A, mu_B, mu_C...\n";
    
    for (double mu_A = 8.0; mu_A <= 12.0; mu_A += 0.5) {
        for (double mu_B = 5.0; mu_B <= 15.0; mu_B += 1.0) {
            for (double mu_C = 5.0; mu_C <= 15.0; mu_C += 1.0) {
                std::vector<double> means = {mu_A, mu_B, mu_C};
                std::vector<double> vars = {4.0, 1.0, 2.0};
                
                auto result = test_max_max_shared(means, vars);
                total_tests++;
                
                if (result.has_negative) {
                    tests_with_negative++;
                }
                
                double error = std::abs(result.sum - 1.0);
                if (error > 1e-6) {
                    tests_sum_not_one++;
                    if (error > max_sum_error) {
                        max_sum_error = error;
                    }
                    if (error < min_sum_error) {
                        min_sum_error = error;
                    }
                }
            }
        }
    }
    
    std::cout << "  Total tests: " << total_tests << "\n";
    std::cout << "  Tests with negative gradients: " << tests_with_negative << "\n";
    std::cout << "  Tests with sum != 1.0: " << tests_sum_not_one << "\n";
    if (tests_sum_not_one > 0) {
        std::cout << "  Max sum error: " << max_sum_error << "\n";
        std::cout << "  Min sum error: " << min_sum_error << "\n";
    }
    std::cout << "\n";
    
    // Test 2: Show specific examples with negative gradients
    std::cout << "Test 2: Specific examples with negative gradients\n";
    std::cout << std::setw(8) << "mu_A" << std::setw(8) << "mu_B" << std::setw(8) << "mu_C"
              << std::setw(12) << "grad_A" << std::setw(12) << "grad_B" << std::setw(12) << "grad_C"
              << std::setw(12) << "Sum" << "\n";
    std::cout << std::string(68, '-') << "\n";
    
    std::vector<std::vector<double>> test_cases = {
        {10.0, 8.0, 12.0},
        {10.0, 7.0, 13.0},
        {9.0, 6.0, 12.0},
        {11.0, 9.0, 14.0},
        {10.0, 5.0, 15.0},
    };
    
    for (const auto& means : test_cases) {
        std::vector<double> vars = {4.0, 1.0, 2.0};
        auto result = test_max_max_shared(means, vars);
        
        std::cout << std::setw(8) << means[0]
                  << std::setw(8) << means[1]
                  << std::setw(8) << means[2]
                  << std::setw(12) << result.grad_mu[0]
                  << std::setw(12) << result.grad_mu[1]
                  << std::setw(12) << result.grad_mu[2]
                  << std::setw(12) << result.sum;
        
        if (result.has_negative) {
            std::cout << " [has negative]";
        }
        if (std::abs(result.sum - 1.0) > 1e-6) {
            std::cout << " ⚠️  ERROR: sum != 1.0";
        }
        std::cout << "\n";
    }
    
    std::cout << "\n";
    
    // Test 3: MAX(MAX(MAX(A, B), MAX(A, C)), MAX(A, D)) - A shared in all
    std::cout << "Test 3: MAX(MAX(MAX(A, B), MAX(A, C)), MAX(A, D)) - A shared in all\n";
    std::cout << "A ~ N(10.0, 4.0), B ~ N(8.0, 1.0), C ~ N(12.0, 2.0), D ~ N(9.0, 3.0)\n";
    
    std::vector<double> means4 = {10.0, 8.0, 12.0, 9.0};
    std::vector<double> vars4 = {4.0, 1.0, 2.0, 3.0};
    auto result4 = test_max_max_max_shared(means4, vars4);
    
    std::cout << "  grad_A = " << result4.grad_mu[0] << "\n";
    std::cout << "  grad_B = " << result4.grad_mu[1] << "\n";
    std::cout << "  grad_C = " << result4.grad_mu[2] << "\n";
    std::cout << "  grad_D = " << result4.grad_mu[3] << "\n";
    std::cout << "  Sum = " << result4.sum << "\n";
    
    if (result4.has_negative) {
        std::cout << "  [Has " << result4.negative_count << " negative gradient(s)]\n";
    }
    if (std::abs(result4.sum - 1.0) > 1e-6) {
        std::cout << "  ⚠️  ERROR: sum != 1.0\n";
    } else {
        std::cout << "  ✓ Sum = 1.0\n";
    }
    
    std::cout << "\n";
    
    // Summary
    std::cout << "=== Summary ===\n";
    if (tests_sum_not_one == 0) {
        std::cout << "✓ PASSED: All tests have gradient sum = 1.0\n";
        std::cout << "✓ Even when negative gradients occur, the sum is always 1.0\n";
        std::cout << "✓ This confirms the mathematical correctness of multi-stage MAX operations\n";
        return 0;
    } else {
        std::cout << "⚠️  FAILED: " << tests_sum_not_one << " tests have sum != 1.0\n";
        return 1;
    }
}

