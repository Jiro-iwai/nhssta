// -*- c++ -*-
// Author: Jiro Iwai

/**
 * @file expression.hpp
 * @brief 自動微分（Reverse-mode AD）による感度解析のための式木クラス
 *
 * ## 概要
 *
 * このクラスは、SSTA における感度解析機能のために設計された式木（Expression Tree）
 * の実装です。逆伝播（Reverse-mode Automatic Differentiation）を用いて、
 * 偏微分 ∂f/∂x を効率的に計算します。
 *
 * ## 現在の状態
 *
 * - **ステータス**: 基本機能実装済み
 * - **SSTA コア機能**: 統合は将来の課題
 * - **テスト**: 全演算の勾配テストあり
 *
 * ## 実装済み機能
 *
 * - 式木の構築（Const, Variable, 算術演算）
 * - 式の値の評価（value()）
 * - **逆伝播による勾配計算（backward(), gradient()）**
 * - 勾配のリセット（zero_grad(), zero_all_grad()）
 * - デバッグ用の式木表示（print(), print_all()）
 *
 * ## 対応演算と勾配
 *
 * | 演算 | 順方向 | ∂/∂left | ∂/∂right |
 * |------|--------|---------|----------|
 * | +    | l + r  | 1       | 1        |
 * | -    | l - r  | 1       | -1       |
 * | *    | l * r  | r       | l        |
 * | /    | l / r  | 1/r     | -l/r²    |
 * | ^    | l^r    | r*l^(r-1) | l^r*log(l) |
 * | exp  | exp(l) | exp(l)  | -        |
 * | log  | log(l) | 1/l     | -        |
 *
 * ## 使用例
 *
 * @code
 * // 変数を定義
 * Variable x, y;
 * x = 2.0;
 * y = 3.0;
 *
 * // 式を構築: f = x * y + x²
 * Expression f = x * y + (x ^ 2.0);  // f = 6 + 4 = 10
 *
 * // 勾配を計算
 * f->backward();
 *
 * // ∂f/∂x = y + 2x = 3 + 4 = 7
 * double df_dx = x->gradient();  // 7.0
 *
 * // ∂f/∂y = x = 2
 * double df_dy = y->gradient();  // 2.0
 *
 * // 次の計算の前に勾配をリセット
 * zero_all_grad();
 * @endcode
 *
 * ## 将来の TODO
 *
 * - RandomVariable との統合（mean/variance の感度計算）
 * - クリティカルパスへの寄与度計算
 *
 * @see test/test_expression_derivative.cpp - 勾配計算のテスト
 * @see test/test_expression_print.cpp - 式木表示のテスト
 * @see Issue #163 - 感度解析機能の検討
 */

#ifndef EXPRESSION__H
#define EXPRESSION__H

#include <cassert>
#include <map>
#include <memory>
#include <nhssta/exception.hpp>
#include <set>
#include <type_traits>

#include "handle.hpp"

////////////////////


class ExpressionImpl;

// Handle pattern for Expression: thin wrapper around std::shared_ptr
//
// Ownership semantics:
// - Copying a Handle is lightweight: it shares ownership via std::shared_ptr
// - Passing by value (Expression) transfers/shares ownership
// - Passing by const reference (const Expression&) is a non-owning reference
// - Storing as member variable creates ownership relationship
//
// Value category:
// - Copy is cheap (only copies shared_ptr, not the underlying object)
// - Move is cheap (transfers shared_ptr ownership)
// - Use const reference for read-only access when ownership is not needed
// ExpressionHandle: Handle using HandleBase template
//
// Ownership semantics:
// - Copying a Handle is lightweight: it shares ownership via std::shared_ptr
// - Passing by value (Expression) transfers/shares ownership
// - Passing by const reference (const Expression&) is a non-owning reference
// - Storing as member variable creates ownership relationship
//
// Value category:
// - Copy is cheap (only copies shared_ptr, not the underlying object)
// - Move is cheap (transfers shared_ptr ownership)
// - Use const reference for read-only access when ownership is not needed
class ExpressionHandle : public HandleBase<ExpressionImpl> {
   public:
    using element_type = ExpressionImpl;

    // Default constructor: creates null handle
    ExpressionHandle() = default;
    
    // Inherit other constructors from HandleBase
    using HandleBase<ExpressionImpl>::HandleBase;
};

// Type alias: Expression is a Handle (thin wrapper around std::shared_ptr)
// Usage:
// - Pass by value when ownership should be shared: void func(Expression expr)
// - Pass by const reference for read-only access: void func(const Expression& expr)
// - Store as member variable to create ownership: Expression left_;
using Expression = ExpressionHandle;

class ExpressionImpl : public std::enable_shared_from_this<ExpressionImpl> {
   public:
    friend class Variable;

    friend Expression operator+(const Expression& a, const Expression& b);
    friend Expression operator+(double a, const Expression& b);
    friend Expression operator+(const Expression& a, double b);
    friend Expression operator+(const Expression& a);

    friend Expression operator-(const Expression& a, const Expression& b);
    friend Expression operator-(double a, const Expression& b);
    friend Expression operator-(const Expression& a, double b);
    friend Expression operator-(const Expression& a);

    friend Expression operator*(const Expression& a, const Expression& b);
    friend Expression operator*(double a, const Expression& b);
    friend Expression operator*(const Expression& a, double b);

    friend Expression operator/(const Expression& a, const Expression& b);
    friend Expression operator/(const Expression& a, double b);
    friend Expression operator/(double a, const Expression& b);

    friend Expression operator^(const Expression& a, const Expression& b);
    friend Expression operator^(const Expression& a, double b);
    friend Expression operator^(double a, const Expression& b);

    friend Expression exp(const Expression& a);
    friend Expression log(const Expression& a);
    friend Expression erf(const Expression& a);
    friend Expression sqrt(const Expression& a);

    // assignment to (double)
    friend double& operator<<(double& a, const Expression& b);

    // print all expression infomation
    friend void print_all();

    enum Op { CONST = 0, PARAM, PLUS, MINUS, MUL, DIV, POWER, EXP, LOG, ERF, SQRT, PHI2 };

    static void print_all();
    void print();

    ExpressionImpl();
    ExpressionImpl(double value);

    ExpressionImpl(const Op& op, const Expression& left, const Expression& right);

    // Constructor for 3-argument operations (e.g., PHI2)
    ExpressionImpl(const Op& op, const Expression& first, const Expression& second,
                   const Expression& third);

    virtual ~ExpressionImpl();

    [[nodiscard]] int id() const {
        return id_;
    }

    // Automatic differentiation (reverse-mode)
    void backward(double upstream = 1.0);
    [[nodiscard]] double gradient() const { return gradient_; }
    void zero_grad();
    static void zero_all_grad();
    static size_t node_count();

    // Value computation
    virtual double value();
    
    // Public accessors for children (needed for topological sort in backward)
    [[nodiscard]] const Expression& left() const {
        return left_;
    }
    [[nodiscard]] const Expression& right() const {
        return right_;
    }
    [[nodiscard]] const Expression& third() const {
        return third_;
    }

   protected:
    // Helper for topological sort backward
    void propagate_gradient();
    void set_value(double value);
    void _set_value(double value);
    void unset_value();
    void unset_root_value();
    [[nodiscard]] bool is_set_value() const {
        return is_set_value_;
    }

    void add_root(ExpressionImpl* root);
    void remove_root(ExpressionImpl* root);

   protected:
    using Expressions = std::set<ExpressionImpl*>;

    int id_;
    bool is_set_value_;
    double value_;

    // Gradient for automatic differentiation (reverse-mode)
    double gradient_;        // Accumulated gradient ∂L/∂this
    bool is_gradient_set_;   // Whether gradient has been computed

    const Op op_;  // NOLINT(cppcoreguidelines-avoid-const-or-ref-data-members) - Op is a handle
                   // type, const is intentional
    Expression left_;
    Expression right_;
    Expression third_;  // For 3-argument operations (e.g., PHI2: h, k, rho)
    Expressions roots_;

    /// static data menber
    static int current_id_;
    
    /// Access to expression table via function to avoid static destruction order issues
    /// Using function-local static ensures proper lifetime management
    static Expressions& eTbl();
};

// print all expression infomation
void print_all();

////////////////

class ConstImpl : public ExpressionImpl {
   public:
    ConstImpl(double value)
        : ExpressionImpl(value) {}
    ~ConstImpl() override = default;

};

class Const : public Expression {
   public:
    Const(double value);
    ~Const() = default;
};

////////////////

class VariableImpl : public ExpressionImpl {
   public:
    VariableImpl() = default;
    ~VariableImpl() override = default;
    double value() override;
};

class Variable : public Expression {
   public:
    Variable();
    ~Variable() = default;

    ////// substitution from (double)
    Variable& operator=(double value);
};

////////////////

Expression operator+(const Expression& a, const Expression& b);
Expression operator+(double a, const Expression& b);
Expression operator+(const Expression& a, double b);
Expression operator+(const Expression& a);

Expression operator-(const Expression& a, const Expression& b);
Expression operator-(double a, const Expression& b);
Expression operator-(const Expression& a, double b);
Expression operator-(const Expression& a);

Expression operator*(const Expression& a, const Expression& b);
Expression operator*(double a, const Expression& b);
Expression operator*(const Expression& a, double b);

Expression operator/(const Expression& a, const Expression& b);
Expression operator/(const Expression& a, double b);
Expression operator/(double a, const Expression& b);

Expression operator^(const Expression& a, const Expression& b);
Expression operator^(const Expression& a, double b);
Expression operator^(double a, const Expression& b);

Expression exp(const Expression& a);
Expression log(const Expression& a);
Expression erf(const Expression& a);
Expression sqrt(const Expression& a);

// Statistical functions for sensitivity analysis
// These are building blocks for SSTA integration

/**
 * @brief Standard normal PDF: φ(x) = exp(-x²/2) / √(2π)
 * 
 * @param x Expression to evaluate
 * @return Expression representing φ(x)
 */
inline Expression phi_expr(const Expression& x) {
    static constexpr double INV_SQRT_2PI = 0.3989422804014327;  // 1/√(2π)
    return INV_SQRT_2PI * exp(-(x * x) / 2.0);
}

/**
 * @brief Standard normal CDF: Φ(x) = 0.5 × (1 + erf(x/√2))
 * 
 * @param x Expression to evaluate
 * @return Expression representing Φ(x)
 */
inline Expression Phi_expr(const Expression& x) {
    static constexpr double INV_SQRT_2 = 0.7071067811865476;  // 1/√2
    return 0.5 * (1.0 + erf(x * INV_SQRT_2));
}

/**
 * @brief MeanMax function: E[max(a, x)] where x ~ N(0,1)
 * MeanMax(a) = φ(a) + a × Φ(a)
 * 
 * @param a Expression for the threshold
 * @return Expression representing MeanMax(a)
 */
inline Expression MeanMax_expr(const Expression& a) {
    return phi_expr(a) + a * Phi_expr(a);
}

/**
 * @brief MeanMax2 function: E[max(a, x)²] where x ~ N(0,1)
 * MeanMax2(a) = 1 + (a² - 1) × Φ(a) + a × φ(a)
 * 
 * @param a Expression for the threshold
 * @return Expression representing MeanMax2(a)
 */
inline Expression MeanMax2_expr(const Expression& a) {
    return 1.0 + (a * a - 1.0) * Phi_expr(a) + a * phi_expr(a);
}

/**
 * @brief MeanPhiMax function: E[max(a, x) × x] where x ~ N(0,1)
 * MeanPhiMax(a) = Φ(-a) = 1 - Φ(a)
 * 
 * @param a Expression for the threshold
 * @return Expression representing MeanPhiMax(a)
 */
inline Expression MeanPhiMax_expr(const Expression& a) {
    return 1.0 - Phi_expr(a);
}

// =============================================================================
// MAX0 functions for SSTA
// =============================================================================

/**
 * @brief Mean of max(0, D) where D ~ N(μ, σ²)
 * 
 * E[max(0, D)] = μ + σ × MeanMax(-μ/σ)
 *              = μ + σ × (φ(a) + a × Φ(a))  where a = -μ/σ
 * 
 * @param mu Mean of the underlying normal distribution
 * @param sigma Standard deviation (NOT variance)
 * @return Expression representing E[max(0, D)]
 */
inline Expression max0_mean_expr(const Expression& mu, const Expression& sigma) {
    Expression a = -(mu / sigma);  // normalized threshold
    return mu + sigma * MeanMax_expr(a);
}

/**
 * @brief Variance of max(0, D) where D ~ N(μ, σ²)
 * 
 * Var[max(0, D)] = σ² × (MeanMax2(a) - MeanMax(a)²)  where a = -μ/σ
 * 
 * @param mu Mean of the underlying normal distribution
 * @param sigma Standard deviation (NOT variance)
 * @return Expression representing Var[max(0, D)]
 */
inline Expression max0_var_expr(const Expression& mu, const Expression& sigma) {
    Expression a = -(mu / sigma);  // normalized threshold
    Expression mm = MeanMax_expr(a);
    Expression mm2 = MeanMax2_expr(a);
    return sigma * sigma * (mm2 - mm * mm);
}

// =============================================================================
// ADD/SUB mean and variance expressions (Phase 3 of #167)
// =============================================================================

/**
 * @brief Mean of sum: E[A + B] = μ_A + μ_B
 */
inline Expression add_mean_expr(const Expression& mu1, const Expression& mu2) {
    return mu1 + mu2;
}

/**
 * @brief Variance of sum: Var[A + B] = σ_A² + σ_B² + 2×Cov(A,B)
 * 
 * @param sigma1 Standard deviation of A
 * @param sigma2 Standard deviation of B
 * @param cov Covariance between A and B
 */
inline Expression add_var_expr(const Expression& sigma1, const Expression& sigma2,
                               const Expression& cov) {
    return sigma1 * sigma1 + sigma2 * sigma2 + Const(2.0) * cov;
}

/**
 * @brief Mean of difference: E[A - B] = μ_A - μ_B
 */
inline Expression sub_mean_expr(const Expression& mu1, const Expression& mu2) {
    return mu1 - mu2;
}

/**
 * @brief Variance of difference: Var[A - B] = σ_A² + σ_B² - 2×Cov(A,B)
 * 
 * @param sigma1 Standard deviation of A
 * @param sigma2 Standard deviation of B
 * @param cov Covariance between A and B
 */
inline Expression sub_var_expr(const Expression& sigma1, const Expression& sigma2,
                               const Expression& cov) {
    return sigma1 * sigma1 + sigma2 * sigma2 - Const(2.0) * cov;
}

// =============================================================================
// Covariance expressions (Phase 4 of #167)
// =============================================================================

/**
 * @brief Cov(X, max0(Z)) where X and Z are jointly Gaussian
 * 
 * Formula: Cov(X, max0(Z)) = Cov(X, Z) × Φ(μ_Z/σ_Z)
 * 
 * This is used when computing variance of MAX operations:
 * Var[MAX(A,B)] = Var[A + MAX0(B-A)] requires Cov terms involving MAX0.
 * 
 * @param cov_xz Covariance between X and Z: Cov(X, Z)
 * @param mu_z Mean of Z
 * @param sigma_z Standard deviation of Z (NOT variance)
 * @return Expression representing Cov(X, max0(Z))
 */
inline Expression cov_x_max0_expr(const Expression& cov_xz,
                                  const Expression& mu_z,
                                  const Expression& sigma_z) {
    // Cov(X, max0(Z)) = Cov(X, Z) × Φ(μ_Z/σ_Z)
    // Using MeanPhiMax(-μ/σ) = Φ(μ/σ)
    return cov_xz * Phi_expr(mu_z / sigma_z);
}

// =============================================================================
// Bivariate normal distribution expressions (Phase 5 of #167)
// =============================================================================

/**
 * @brief Bivariate standard normal PDF φ₂(x, y; ρ)
 * 
 * Formula: φ₂(x, y; ρ) = 1/(2π√(1-ρ²)) × exp(-(x² - 2ρxy + y²)/(2(1-ρ²)))
 * 
 * Gradients (derived from the formula):
 * - ∂φ₂/∂x = φ₂ × (-(x - ρy)/(1-ρ²))
 * - ∂φ₂/∂y = φ₂ × (-(y - ρx)/(1-ρ²))
 * - ∂φ₂/∂ρ = φ₂ × [ρ/(1-ρ²) + (x² - 2ρxy + y²)×ρ/(1-ρ²)² - xy/(1-ρ²)]
 * 
 * @param x First variable
 * @param y Second variable
 * @param rho Correlation coefficient (-1 < ρ < 1)
 */
Expression phi2_expr(const Expression& x, const Expression& y, const Expression& rho);

/**
 * @brief Bivariate standard normal CDF Φ₂(h, k; ρ)
 * 
 * Φ₂(h, k; ρ) = P(X ≤ h, Y ≤ k) where (X, Y) ~ N(0, 0, 1, 1, ρ)
 * 
 * Value: Computed using numerical integration (Gauss-Hermite)
 * Gradients (analytical):
 * - ∂Φ₂/∂h = φ(h) × Φ((k - ρh)/√(1-ρ²))
 * - ∂Φ₂/∂k = φ(k) × Φ((h - ρk)/√(1-ρ²))
 * - ∂Φ₂/∂ρ = φ₂(h, k; ρ)
 * 
 * @param h First threshold
 * @param k Second threshold
 * @param rho Correlation coefficient (-1 < ρ < 1)
 */
Expression Phi2_expr(const Expression& h, const Expression& k, const Expression& rho);

/**
 * @brief E[D0⁺ × D1⁺] where D0, D1 are bivariate normal
 * 
 * Formula:
 * E[D0⁺ D1⁺] = μ0 μ1 Φ₂(a0, a1; ρ) 
 *            + μ0 σ1 φ(a1) Φ((a0 - ρa1)/√(1-ρ²))
 *            + μ1 σ0 φ(a0) Φ((a1 - ρa0)/√(1-ρ²))
 *            + σ0 σ1 [ρ Φ₂(a0, a1; ρ) + (1-ρ²) φ₂(a0, a1; ρ)]
 * 
 * where a0 = μ0/σ0, a1 = μ1/σ1
 * 
 * @param mu0 Mean of D0
 * @param sigma0 Standard deviation of D0 (> 0)
 * @param mu1 Mean of D1
 * @param sigma1 Standard deviation of D1 (> 0)
 * @param rho Correlation between D0 and D1
 */
Expression expected_prod_pos_expr(const Expression& mu0, const Expression& sigma0,
                                  const Expression& mu1, const Expression& sigma1,
                                  const Expression& rho);

/**
 * @brief E[D0⁺ D1⁺] for ρ = 1 (perfectly correlated)
 *
 * When ρ = 1: D0 = μ0 + σ0·Z, D1 = μ1 + σ1·Z (same Z)
 * Both positive when Z > c where c = -min(a0, a1)
 * E[D0⁺ D1⁺] = σ0·σ1 · [(a0·a1 + 1)·Φ(-c) + (a0 + a1 + c)·φ(c)]
 */
Expression expected_prod_pos_rho1_expr(const Expression& mu0, const Expression& sigma0,
                                       const Expression& mu1, const Expression& sigma1);

/**
 * @brief E[D0⁺ D1⁺] for ρ = -1 (perfectly negatively correlated)
 *
 * When ρ = -1: D0 = μ0 + σ0·Z, D1 = μ1 - σ1·Z (opposite signs)
 * Both positive when -a0 < Z < a1 (if a0 + a1 > 0)
 * Returns 0 if a0 + a1 <= 0 (interval is empty)
 */
Expression expected_prod_pos_rho_neg1_expr(const Expression& mu0, const Expression& sigma0,
                                           const Expression& mu1, const Expression& sigma1);

/**
 * @brief Cov(max(0,D0), max(0,D1)) where D0, D1 are bivariate normal
 * 
 * Formula: Cov(D0⁺, D1⁺) = E[D0⁺ D1⁺] - E[D0⁺] × E[D1⁺]
 * 
 * @param mu0 Mean of D0
 * @param sigma0 Standard deviation of D0 (> 0)
 * @param mu1 Mean of D1
 * @param sigma1 Standard deviation of D1 (> 0)
 * @param rho Correlation between D0 and D1
 */
Expression cov_max0_max0_expr(const Expression& mu0, const Expression& sigma0,
                              const Expression& mu1, const Expression& sigma1,
                              const Expression& rho);

// assignment to (double)
double& operator<<(double& a, const Expression& b);

// Automatic differentiation utilities
void zero_all_grad();

#endif  // EXPRESSION__H
