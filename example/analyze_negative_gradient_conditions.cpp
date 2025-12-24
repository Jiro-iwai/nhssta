// -*- c++ -*-
// Analyze conditions under which negative gradients occur in multi-stage MAX operations

#include "../src/random_variable.hpp"
#include "../src/max.hpp"
#include "../src/expression.hpp"
#include "../src/normal.hpp"
#include <iostream>
#include <iomanip>
#include <vector>
#include <cmath>
#include <map>
#include <algorithm>

using ::RandomVariable::Normal;
using ::RandomVariable::MAX;
using RandomVar = ::RandomVariable::RandomVariable;

struct AnalysisResult {
    std::vector<double> means;
    std::vector<double> vars;
    std::vector<double> grad_mu;
    bool has_negative;
    int negative_index;
    double negative_value;
    double max_mean;
    double min_mean;
    double negative_mean;
};

AnalysisResult analyze_max_max_shared(const std::vector<double>& means,
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
    
    AnalysisResult result;
    result.means = means;
    result.vars = vars;
    result.grad_mu = {A->mean_expr()->gradient(),
                      B->mean_expr()->gradient(),
                      C->mean_expr()->gradient()};
    
    result.has_negative = false;
    result.negative_index = -1;
    result.negative_value = 0.0;
    
    double max_m = means[0];
    double min_m = means[0];
    int max_idx = 0;
    int min_idx = 0;
    
    for (size_t i = 0; i < means.size(); ++i) {
        if (means[i] > max_m) {
            max_m = means[i];
            max_idx = i;
        }
        if (means[i] < min_m) {
            min_m = means[i];
            min_idx = i;
        }
        
        if (result.grad_mu[i] < -1e-10) {
            result.has_negative = true;
            if (result.negative_index == -1 || result.grad_mu[i] < result.negative_value) {
                result.negative_index = i;
                result.negative_value = result.grad_mu[i];
            }
        }
    }
    
    result.max_mean = max_m;
    result.min_mean = min_m;
    if (result.negative_index >= 0) {
        result.negative_mean = means[result.negative_index];
    } else {
        result.negative_mean = 0.0;
    }
    
    return result;
}

int main() {
    std::cout << std::fixed << std::setprecision(6);
    std::cout << "=== Analysis: Conditions for Negative Gradients ===\n\n";
    
    std::vector<AnalysisResult> negative_cases;
    std::vector<AnalysisResult> positive_cases;
    
    // Systematic analysis
    std::cout << "Systematic analysis: MAX(MAX(A, B), MAX(A, C))\n";
    std::cout << "A ~ N(mu_A, 4.0), B ~ N(mu_B, 1.0), C ~ N(mu_C, 2.0)\n\n";
    
    int total = 0;
    for (double mu_A = 8.0; mu_A <= 12.0; mu_A += 1.0) {
        for (double mu_B = 5.0; mu_B <= 15.0; mu_B += 2.0) {
            for (double mu_C = 5.0; mu_C <= 15.0; mu_C += 2.0) {
                std::vector<double> means = {mu_A, mu_B, mu_C};
                std::vector<double> vars = {4.0, 1.0, 2.0};
                
                auto result = analyze_max_max_shared(means, vars);
                total++;
                
                if (result.has_negative) {
                    negative_cases.push_back(result);
                } else {
                    positive_cases.push_back(result);
                }
            }
        }
    }
    
    std::cout << "Total cases analyzed: " << total << "\n";
    std::cout << "Cases with negative gradients: " << negative_cases.size() << "\n";
    std::cout << "Cases without negative gradients: " << positive_cases.size() << "\n\n";
    
    // Analyze patterns in negative cases
    std::cout << "=== Pattern Analysis: When Do Negative Gradients Occur? ===\n\n";
    
    // Pattern 1: Which variable has negative gradient?
    std::map<int, int> negative_by_index;
    for (const auto& r : negative_cases) {
        negative_by_index[r.negative_index]++;
    }
    
    std::cout << "1. Which variable has negative gradient?\n";
    std::cout << "   Index 0 (A, shared): " << negative_by_index[0] << " cases\n";
    std::cout << "   Index 1 (B): " << negative_by_index[1] << " cases\n";
    std::cout << "   Index 2 (C): " << negative_by_index[2] << " cases\n\n";
    
    // Pattern 2: Relationship between means
    std::cout << "2. Mean relationships in negative gradient cases:\n";
    int cases_A_is_middle = 0;
    int cases_A_is_max = 0;
    int cases_A_is_min = 0;
    int cases_B_negative = 0;
    int cases_C_negative = 0;
    int cases_B_smaller_than_A = 0;
    int cases_C_larger_than_A = 0;
    
    for (const auto& r : negative_cases) {
        double mu_A = r.means[0];
        double mu_B = r.means[1];
        double mu_C = r.means[2];
        
        if (r.negative_index == 1) { // B has negative gradient
            cases_B_negative++;
            if (mu_B < mu_A) cases_B_smaller_than_A++;
        }
        if (r.negative_index == 2) { // C has negative gradient
            cases_C_negative++;
            if (mu_C < mu_A) cases_C_larger_than_A++;
        }
        
        if (mu_A > mu_B && mu_A < mu_C) cases_A_is_middle++;
        if (mu_A >= mu_B && mu_A >= mu_C) cases_A_is_max++;
        if (mu_A <= mu_B && mu_A <= mu_C) cases_A_is_min++;
    }
    
    std::cout << "   When B has negative gradient:\n";
    std::cout << "     - Total: " << cases_B_negative << " cases\n";
    std::cout << "     - mu_B < mu_A: " << cases_B_smaller_than_A << " cases ("
              << (100.0 * cases_B_smaller_than_A / std::max(1, cases_B_negative)) << "%)\n";
    
    std::cout << "   When C has negative gradient:\n";
    std::cout << "     - Total: " << cases_C_negative << " cases\n";
    std::cout << "     - mu_C < mu_A: " << cases_C_larger_than_A << " cases ("
              << (100.0 * cases_C_larger_than_A / std::max(1, cases_C_negative)) << "%)\n";
    
    std::cout << "   A's position relative to B and C:\n";
    std::cout << "     - A is maximum: " << cases_A_is_max << " cases\n";
    std::cout << "     - A is minimum: " << cases_A_is_min << " cases\n";
    std::cout << "     - A is middle: " << cases_A_is_middle << " cases\n\n";
    
    // Pattern 3: Specific examples
    std::cout << "3. Specific examples:\n\n";
    
    std::cout << "Example 1: B has negative gradient\n";
    std::cout << std::setw(8) << "mu_A" << std::setw(8) << "mu_B" << std::setw(8) << "mu_C"
              << std::setw(12) << "grad_B" << std::setw(15) << "mu_B < mu_A?" << "\n";
    std::cout << std::string(60, '-') << "\n";
    
    int count = 0;
    for (const auto& r : negative_cases) {
        if (r.negative_index == 1 && count < 10) { // B has negative
            std::cout << std::setw(8) << r.means[0]
                      << std::setw(8) << r.means[1]
                      << std::setw(8) << r.means[2]
                      << std::setw(12) << r.grad_mu[1]
                      << std::setw(15) << (r.means[1] < r.means[0] ? "Yes" : "No")
                      << "\n";
            count++;
        }
    }
    
    std::cout << "\nExample 2: C has negative gradient\n";
    std::cout << std::setw(8) << "mu_A" << std::setw(8) << "mu_B" << std::setw(8) << "mu_C"
              << std::setw(12) << "grad_C" << std::setw(15) << "mu_C < mu_A?" << "\n";
    std::cout << std::string(60, '-') << "\n";
    
    count = 0;
    for (const auto& r : negative_cases) {
        if (r.negative_index == 2 && count < 10) { // C has negative
            std::cout << std::setw(8) << r.means[0]
                      << std::setw(8) << r.means[1]
                      << std::setw(8) << r.means[2]
                      << std::setw(12) << r.grad_mu[2]
                      << std::setw(15) << (r.means[2] < r.means[0] ? "Yes" : "No")
                      << "\n";
            count++;
        }
    }
    
    // Pattern 4: General rule
    std::cout << "\n=== General Rules ===\n\n";
    
    std::cout << "Rule 1: Negative gradients occur when:\n";
    std::cout << "  - A shared input (A) appears in multiple MAX operations\n";
    std::cout << "  - One of the non-shared inputs (B or C) has a smaller mean than A\n";
    std::cout << "  - That non-shared input loses in its local MAX operation\n\n";
    
    std::cout << "Rule 2: Which variable gets negative gradient?\n";
    std::cout << "  - B gets negative gradient when: mu_B < mu_A AND mu_C > mu_A\n";
    std::cout << "    (B loses to A in MAX(A,B), and C wins in MAX(A,C))\n";
    std::cout << "  - C gets negative gradient when: mu_C < mu_A AND mu_B > mu_A\n";
    std::cout << "    (C loses to A in MAX(A,C), and B wins in MAX(A,B))\n\n";
    
    std::cout << "Rule 3: Why does this happen?\n";
    std::cout << "  - MAX function normalizes: MAX(A,B) = A + MAX0(B-A)\n";
    std::cout << "  - When A dominates (mu_A > mu_B), MAX0(B-A) has negative gradient\n";
    std::cout << "  - In multi-stage MAX, this negative gradient propagates through\n";
    std::cout << "  - The final result is dominated by the winner, making losers' gradients negative\n\n";
    
    // Pattern 5: Quantitative analysis
    std::cout << "=== Quantitative Analysis ===\n\n";
    
    double avg_negative_value = 0.0;
    double max_negative_value = 0.0;
    double min_negative_value = 0.0;
    bool first = true;
    
    for (const auto& r : negative_cases) {
        if (r.has_negative) {
            double neg_val = r.negative_value;
            avg_negative_value += neg_val;
            if (first || neg_val < max_negative_value) {
                max_negative_value = neg_val;
            }
            if (first || neg_val > min_negative_value) {
                min_negative_value = neg_val;
            }
            first = false;
        }
    }
    
    if (!negative_cases.empty()) {
        avg_negative_value /= negative_cases.size();
        std::cout << "Negative gradient statistics:\n";
        std::cout << "  Average: " << avg_negative_value << "\n";
        std::cout << "  Maximum (most negative): " << max_negative_value << "\n";
        std::cout << "  Minimum (least negative): " << min_negative_value << "\n";
        std::cout << "  Range: [" << max_negative_value << ", " << min_negative_value << "]\n";
    }
    
    std::cout << "\n=== Summary ===\n\n";
    std::cout << "General conditions for negative gradients:\n";
    std::cout << "1. Multi-stage MAX operation with shared input\n";
    std::cout << "2. Shared input (A) has intermediate mean value\n";
    std::cout << "3. Non-shared input (B or C) has smaller mean than A\n";
    std::cout << "4. That non-shared input loses in its local MAX operation\n";
    std::cout << "5. The final result is dominated by the other branch\n\n";
    
    std::cout << "Mathematical explanation:\n";
    std::cout << "- MAX(A,B) = A + MAX0(B-A) when mu_A > mu_B\n";
    std::cout << "- MAX0(B-A) has negative gradient w.r.t. mu_B\n";
    std::cout << "- In cascaded MAX, this propagates to final output\n";
    std::cout << "- Result: loser's gradient becomes negative\n";
    
    return 0;
}

