// -*- c++ -*-
// Monte Carlo verification for MAX(MAX(A,B), MAX(A,C))
// Compute exact solution and sensitivity analysis

#include <random>
#include <iostream>
#include <iomanip>
#include <vector>
#include <algorithm>
#include <cmath>
#include <numeric>

// Simple max function
double max(double a, double b) {
    return (a > b) ? a : b;
}

// Monte Carlo simulation for MAX(MAX(A,B), MAX(A,C))
struct MonteCarloResult {
    double mean;
    double variance;
    double grad_mu_A;
    double grad_mu_B;
    double grad_mu_C;
};

MonteCarloResult monte_carlo_max_max(
    double mu_A, double sigma_A,
    double mu_B, double sigma_B,
    double mu_C, double sigma_C,
    size_t n_samples = 1000000,
    double delta = 1e-4) {
    
    std::random_device rd;
    std::mt19937 gen(rd());
    
    // Pre-generate standard normal random numbers for common random number method
    std::vector<double> rand_A(n_samples), rand_B(n_samples), rand_C(n_samples);
    std::normal_distribution<double> dist_std(0.0, 1.0);
    for (size_t i = 0; i < n_samples; ++i) {
        rand_A[i] = dist_std(gen);
        rand_B[i] = dist_std(gen);
        rand_C[i] = dist_std(gen);
    }
    
    // Main simulation
    std::vector<double> samples;
    samples.reserve(n_samples);
    
    for (size_t i = 0; i < n_samples; ++i) {
        double a = mu_A + sigma_A * rand_A[i];
        double b = mu_B + sigma_B * rand_B[i];
        double c = mu_C + sigma_C * rand_C[i];
        
        double max_ab = max(a, b);
        double max_ac = max(a, c);
        double max_abc = max(max_ab, max_ac);
        
        samples.push_back(max_abc);
    }
    
    // Compute mean and variance
    double mean = std::accumulate(samples.begin(), samples.end(), 0.0) / n_samples;
    double variance = 0.0;
    for (double x : samples) {
        double diff = x - mean;
        variance += diff * diff;
    }
    variance /= (n_samples - 1);
    
    // Sensitivity analysis using finite difference with common random numbers
    // Use same random numbers for better accuracy
    // ∂E[MAX]/∂μ_B ≈ (E[MAX(μ_B+δ)] - E[MAX(μ_B-δ)]) / (2δ)
    
    // Compute E[MAX] for μ_B + δ (using common random numbers)
    double mean_plus = 0.0;
    for (size_t i = 0; i < n_samples; ++i) {
        double a = mu_A + sigma_A * rand_A[i];
        double b = (mu_B + delta) + sigma_B * rand_B[i];
        double c = mu_C + sigma_C * rand_C[i];
        
        double max_ab = max(a, b);
        double max_ac = max(a, c);
        double max_abc = max(max_ab, max_ac);
        
        mean_plus += max_abc;
    }
    mean_plus /= n_samples;
    
    // Compute E[MAX] for μ_B - δ (using common random numbers)
    double mean_minus = 0.0;
    for (size_t i = 0; i < n_samples; ++i) {
        double a = mu_A + sigma_A * rand_A[i];
        double b = (mu_B - delta) + sigma_B * rand_B[i];
        double c = mu_C + sigma_C * rand_C[i];
        
        double max_ab = max(a, b);
        double max_ac = max(a, c);
        double max_abc = max(max_ab, max_ac);
        
        mean_minus += max_abc;
    }
    mean_minus /= n_samples;
    
    double grad_mu_B = (mean_plus - mean_minus) / (2.0 * delta);
    
    // Compute gradients for μ_A and μ_C similarly (using common random numbers)
    double mean_A_plus = 0.0;
    for (size_t i = 0; i < n_samples; ++i) {
        double a = (mu_A + delta) + sigma_A * rand_A[i];
        double b = mu_B + sigma_B * rand_B[i];
        double c = mu_C + sigma_C * rand_C[i];
        
        double max_ab = max(a, b);
        double max_ac = max(a, c);
        double max_abc = max(max_ab, max_ac);
        
        mean_A_plus += max_abc;
    }
    mean_A_plus /= n_samples;
    
    double mean_A_minus = 0.0;
    for (size_t i = 0; i < n_samples; ++i) {
        double a = (mu_A - delta) + sigma_A * rand_A[i];
        double b = mu_B + sigma_B * rand_B[i];
        double c = mu_C + sigma_C * rand_C[i];
        
        double max_ab = max(a, b);
        double max_ac = max(a, c);
        double max_abc = max(max_ab, max_ac);
        
        mean_A_minus += max_abc;
    }
    mean_A_minus /= n_samples;
    
    double grad_mu_A = (mean_A_plus - mean_A_minus) / (2.0 * delta);
    
    double mean_C_plus = 0.0;
    for (size_t i = 0; i < n_samples; ++i) {
        double a = mu_A + sigma_A * rand_A[i];
        double b = mu_B + sigma_B * rand_B[i];
        double c = (mu_C + delta) + sigma_C * rand_C[i];
        
        double max_ab = max(a, b);
        double max_ac = max(a, c);
        double max_abc = max(max_ab, max_ac);
        
        mean_C_plus += max_abc;
    }
    mean_C_plus /= n_samples;
    
    double mean_C_minus = 0.0;
    for (size_t i = 0; i < n_samples; ++i) {
        double a = mu_A + sigma_A * rand_A[i];
        double b = mu_B + sigma_B * rand_B[i];
        double c = (mu_C - delta) + sigma_C * rand_C[i];
        
        double max_ab = max(a, b);
        double max_ac = max(a, c);
        double max_abc = max(max_ab, max_ac);
        
        mean_C_minus += max_abc;
    }
    mean_C_minus /= n_samples;
    
    double grad_mu_C = (mean_C_plus - mean_C_minus) / (2.0 * delta);
    
    MonteCarloResult result;
    result.mean = mean;
    result.variance = variance;
    result.grad_mu_A = grad_mu_A;
    result.grad_mu_B = grad_mu_B;
    result.grad_mu_C = grad_mu_C;
    
    return result;
}

int main() {
    std::cout << std::fixed << std::setprecision(8);
    std::cout << "=== Monte Carlo Verification ===\n\n";
    
    // Test case: A ~ N(10.0, 4.0), B ~ N(8.0, 1.0), C ~ N(12.0, 2.0)
    double mu_A = 10.0;
    double var_A = 4.0;
    double sigma_A = sqrt(var_A);
    
    double mu_B = 8.0;
    double var_B = 1.0;
    double sigma_B = sqrt(var_B);
    
    double mu_C = 12.0;
    double var_C = 2.0;
    double sigma_C = sqrt(var_C);
    
    std::cout << "Input parameters:\n";
    std::cout << "  A ~ N(" << mu_A << ", " << var_A << ")\n";
    std::cout << "  B ~ N(" << mu_B << ", " << var_B << ")\n";
    std::cout << "  C ~ N(" << mu_C << ", " << var_C << ")\n\n";
    
    std::cout << "Running Monte Carlo simulation...\n";
    std::cout << "  This may take a while (computing gradients requires 6× more samples)...\n\n";
    
    size_t n_samples = 1000000;
    MonteCarloResult result = monte_carlo_max_max(
        mu_A, sigma_A,
        mu_B, sigma_B,
        mu_C, sigma_C,
        n_samples);
    
    std::cout << "Monte Carlo Results (n=" << n_samples << "):\n";
    std::cout << "  E[MAX(MAX(A,B), MAX(A,C))] = " << result.mean << "\n";
    std::cout << "  Var[MAX(MAX(A,B), MAX(A,C))] = " << result.variance << "\n\n";
    
    std::cout << "Sensitivity Analysis (finite difference):\n";
    std::cout << "  ∂E[MAX]/∂μ_A = " << result.grad_mu_A;
    if (result.grad_mu_A < -1e-10) {
        std::cout << " [NEGATIVE!]";
    } else {
        std::cout << " [OK]";
    }
    std::cout << "\n";
    
    std::cout << "  ∂E[MAX]/∂μ_B = " << result.grad_mu_B;
    if (result.grad_mu_B < -1e-10) {
        std::cout << " [NEGATIVE!]";
    } else {
        std::cout << " [OK]";
    }
    std::cout << "\n";
    
    std::cout << "  ∂E[MAX]/∂μ_C = " << result.grad_mu_C;
    if (result.grad_mu_C < -1e-10) {
        std::cout << " [NEGATIVE!]";
    } else {
        std::cout << " [OK]";
    }
    std::cout << "\n";
    
    double sum = result.grad_mu_A + result.grad_mu_B + result.grad_mu_C;
    std::cout << "  Sum = " << sum << "\n\n";
    
    std::cout << "Comparison with Clark approximation:\n";
    std::cout << "  (Clark approximation results should be compared separately)\n";
    std::cout << "  If grad_B is negative in Monte Carlo, it's not a bug but a property\n";
    std::cout << "  If grad_B is positive in Monte Carlo but negative in Clark, it's a bug\n";
    
    return 0;
}

