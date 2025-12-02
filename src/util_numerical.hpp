// -*- c++ -*-
// Author: IWAI Jiro

#ifndef NH_UTIL_NUMERICAL__H
#define NH_UTIL_NUMERICAL__H

#include <string>

namespace RandomVariable {

double MeanMax(double a);
double MeanMax2(double a);
double MeanPhiMax(double a);

// Standard normal distribution functions
// φ(x) = N(0,1) PDF = exp(-x²/2) / √(2π)
double normal_pdf(double x);

// Φ(x) = N(0,1) CDF = 0.5 * erfc(-x/√2)
double normal_cdf(double x);

// E[max(0, D)] where D ~ N(μ, σ²)
// Formula: E[max(0,D)] = σφ(μ/σ) + μΦ(μ/σ)
// Precondition: sigma > 0
double expected_positive_part(double mu, double sigma);

// E[D0⁺ D1⁺] where D0, D1 are bivariate normal with correlation ρ
// Uses 1D Gauss-Hermite quadrature for numerical integration
// Precondition: sigma0 > 0, sigma1 > 0
// Note: rho is clamped to [-1, 1] internally
double expected_prod_pos(double mu0, double sigma0,
                         double mu1, double sigma1,
                         double rho);

// Cov(max(0,D0), max(0,D1)) where D0, D1 are bivariate normal with correlation ρ
// Formula: E[D0⁺ D1⁺] - E[D0⁺] × E[D1⁺]
// Precondition: sigma0 > 0, sigma1 > 0
double covariance_max0_max0(double mu0, double sigma0,
                            double mu1, double sigma1,
                            double rho);

// Bivariate normal distribution functions
// φ₂(x, y; ρ) = Bivariate normal PDF
double bivariate_normal_pdf(double x, double y, double rho);

// Φ₂(h, k; ρ) = Bivariate normal CDF using Simpson's rule
// 128 points provides 8-digit accuracy with good performance (~1.6μs)
double bivariate_normal_cdf(double h, double k, double rho, int n_points = 128);

// E[D0⁺ D1⁺] for ρ = 1 (perfectly correlated)
// Uses exact std::min (no approximation)
// Precondition: sigma0 > 0, sigma1 > 0
double expected_prod_pos_rho1(double mu0, double sigma0,
                               double mu1, double sigma1);

// E[D0⁺ D1⁺] for ρ = -1 (perfectly negatively correlated)
// Uses exact condition check (no approximation)
// Precondition: sigma0 > 0, sigma1 > 0
double expected_prod_pos_rho_neg1(double mu0, double sigma0,
                                  double mu1, double sigma1);

}  // namespace RandomVariable

#endif  // NH_UTIL_NUMERICAL__H
