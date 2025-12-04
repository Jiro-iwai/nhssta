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
#include <functional>
#include <map>
#include <memory>
#include <nhssta/exception.hpp>
#include <set>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <unordered_set>
#include <vector>

#include "handle.hpp"

////////////////////

// Forward declarations
class ExpressionImpl;
class CustomFunctionImpl;
using CustomFunctionHandle = std::shared_ptr<CustomFunctionImpl>;

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

    enum Op { CONST = 0, PARAM, PLUS, MINUS, MUL, DIV, POWER, EXP, LOG, ERF, SQRT, CUSTOM_FUNCTION };

    static void print_all();
    void print();

    ExpressionImpl();
    ExpressionImpl(double value);

    ExpressionImpl(const Op& op, const Expression& left, const Expression& right);

    // Constructor for CUSTOM_FUNCTION operation
    ExpressionImpl(const CustomFunctionHandle& func,
                   const std::vector<Expression>& args);

    virtual ~ExpressionImpl();

    [[nodiscard]] int id() const {
        return id_;
    }

    [[nodiscard]] Op op() const noexcept {
        return op_;
    }

    // Automatic differentiation (reverse-mode)
    void backward(double upstream = 1.0);
    [[nodiscard]] double gradient() const { return gradient_; }
    void zero_grad();
    static void zero_all_grad();
    static size_t node_count();
    
    // Debug support for backward()
    static void enable_backward_debug(bool enable, const std::string& log_file = "backward_debug.log");

    // Value computation
    virtual double value();
    
    // Public accessors for children (needed for topological sort in backward)
    [[nodiscard]] const Expression& left() const {
        return left_;
    }
    [[nodiscard]] const Expression& right() const {
        return right_;
    }
    [[nodiscard]] const std::vector<Expression>& custom_args() const {
        return custom_args_;
    }

   protected:
    // Helper for topological sort backward
    void propagate_gradient();
    void set_value(double value);
    void _set_value(double value);
    void unset_root_value();

   public:
    void unset_value();  // Made public for CustomFunctionImpl
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
    Expressions roots_;

    // Custom function fields (only valid when op_ == CUSTOM_FUNCTION)
    CustomFunctionHandle custom_func_;
    std::vector<Expression> custom_args_;  // Argument Expressions from main expression tree

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
// Moved to statistical_functions.hpp for better code organization


// assignment to (double)
double& operator<<(double& a, const Expression& b);

// Automatic differentiation utilities
void zero_all_grad();

////////////////
// Custom Function Support

namespace RandomVariable {

// Builder type: function that takes local Variables and returns an Expression
using CustomFunctionBuilder =
    std::function<Expression(const std::vector<Variable>&)>;

// Native function types (for Native type custom functions)
using NativeValueFunc =
    std::function<double(const std::vector<double>&)>;

using NativeGradFunc =
    std::function<std::vector<double>(const std::vector<double>&)>;

using NativeValueGradFunc =
    std::function<std::pair<double, std::vector<double>>(
        const std::vector<double>&)>;

}  // namespace RandomVariable

// Internal implementation class for custom functions
class CustomFunctionImpl {
public:
    enum class ImplKind {
        Graph,
        Native
    };

    using Builder = RandomVariable::CustomFunctionBuilder;
    using NativeValueFunc = RandomVariable::NativeValueFunc;
    using NativeGradFunc = RandomVariable::NativeGradFunc;
    using NativeValueGradFunc = RandomVariable::NativeValueGradFunc;

    // Graph type constructor (existing)
    CustomFunctionImpl(size_t input_dim,
                       const Builder& builder,
                       const std::string& name = "");

    // Native type constructors (new)
    CustomFunctionImpl(size_t input_dim,
                       NativeValueFunc value,
                       NativeGradFunc gradient,
                       const std::string& name = "");

    CustomFunctionImpl(size_t input_dim,
                       NativeValueGradFunc value_and_gradient,
                       const std::string& name = "");

    [[nodiscard]] size_t input_dim() const noexcept { return input_dim_; }
    [[nodiscard]] const std::string& name() const noexcept { return name_; }
    [[nodiscard]] ImplKind kind() const noexcept { return kind_; }

    // Independent evaluation API
    double value(const std::vector<double>& x);
    std::vector<double> gradient(const std::vector<double>& x);
    std::pair<double, std::vector<double>>
    value_and_gradient(const std::vector<double>& x);

    // Called from main expression tree
    std::pair<double, std::vector<double>>
    eval_with_gradient(const std::vector<double>& args_values);

private:
    // Common fields
    size_t input_dim_;
    std::string name_;
    ImplKind kind_;

    // Cache for last evaluated arguments and value (Graph / Native common)
    std::vector<double> last_args_;
    double last_value_{0.0};
    bool has_cached_value_{false};

    // Graph type fields
    std::vector<Variable> local_vars_;  // Internal input variables
    Expression output_;                  // Internal expression tree output
    std::vector<ExpressionImpl*> nodes_; // List of all nodes in internal expression tree (built once)

    // Native type fields
    NativeValueFunc native_value_;      // null if not used
    NativeGradFunc native_grad_;       // null if not used
    NativeValueGradFunc native_value_grad_; // null if not used

    // Helper methods
    void build_nodes_list();
    void collect_nodes_dfs(ExpressionImpl* node,
                           std::unordered_set<ExpressionImpl*>& visited);
    void set_inputs_and_clear(const std::vector<double>& x);
    [[nodiscard]] static bool args_equal(const std::vector<double>& a,
                                         const std::vector<double>& b);
};

// User-facing wrapper class
using CustomFunctionHandle = std::shared_ptr<CustomFunctionImpl>;

class CustomFunction {
public:
    using Builder = CustomFunctionImpl::Builder;
    using GraphBuilder = RandomVariable::CustomFunctionBuilder;
    using NativeValueFunc = RandomVariable::NativeValueFunc;
    using NativeGradFunc = RandomVariable::NativeGradFunc;
    using NativeValueGradFunc = RandomVariable::NativeValueGradFunc;

    CustomFunction() = default;

    explicit CustomFunction(CustomFunctionHandle impl)
        : impl_(std::move(impl)) {}

    // Graph type factory method (existing, renamed for clarity)
    static CustomFunction create_graph(size_t input_dim,
                                       const GraphBuilder& builder,
                                       const std::string& name = "") {
        return CustomFunction(
            std::make_shared<CustomFunctionImpl>(
                input_dim, builder, name));
    }

    // Compatibility: existing create() method
    static CustomFunction create(size_t input_dim,
                                 const Builder& builder,
                                 const std::string& name = "") {
        return create_graph(input_dim, builder, name);
    }

    // Native type factory methods (new)
    static CustomFunction create_native(size_t input_dim,
                                        NativeValueFunc value,
                                        NativeGradFunc gradient,
                                        const std::string& name = "") {
        return CustomFunction(
            std::make_shared<CustomFunctionImpl>(
                input_dim, std::move(value), std::move(gradient), name));
    }

    static CustomFunction create_native(size_t input_dim,
                                        NativeValueGradFunc value_and_gradient,
                                        const std::string& name = "") {
        return CustomFunction(
            std::make_shared<CustomFunctionImpl>(
                input_dim, std::move(value_and_gradient), name));
    }

    [[nodiscard]] bool valid() const noexcept { return static_cast<bool>(impl_); }
    explicit operator bool() const noexcept { return valid(); }

    [[nodiscard]] size_t input_dim() const {
        ensure_valid();
        return impl_->input_dim();
    }

    [[nodiscard]] const std::string& name() const {
        ensure_valid();
        return impl_->name();
    }

    // Independent evaluation API
    [[nodiscard]] double value(const std::vector<double>& x) const {
        ensure_valid();
        return impl_->value(x);
    }

    [[nodiscard]] std::vector<double> gradient(const std::vector<double>& x) const {
        ensure_valid();
        return impl_->gradient(x);
    }

    [[nodiscard]] std::pair<double, std::vector<double>>
    value_and_gradient(const std::vector<double>& x) const {
        ensure_valid();
        return impl_->value_and_gradient(x);
    }

    // Call from main expression tree: operator()
    Expression operator()(const std::vector<Expression>& args) const;
    Expression operator()(std::initializer_list<Expression> args) const;

    // Convenience overloads for 1-7 arguments
    Expression operator()(const Expression& a) const {
        return (*this)({a});
    }

    Expression operator()(const Expression& a, const Expression& b) const {
        return (*this)({a, b});
    }

    Expression operator()(const Expression& a, const Expression& b,
                          const Expression& c) const {
        return (*this)({a, b, c});
    }

    Expression operator()(const Expression& a, const Expression& b,
                          const Expression& c, const Expression& d) const {
        return (*this)({a, b, c, d});
    }

    Expression operator()(const Expression& a, const Expression& b,
                          const Expression& c, const Expression& d,
                          const Expression& e) const {
        return (*this)({a, b, c, d, e});
    }

    Expression operator()(const Expression& a, const Expression& b,
                          const Expression& c, const Expression& d,
                          const Expression& e, const Expression& f) const {
        return (*this)({a, b, c, d, e, f});
    }

    Expression operator()(const Expression& a, const Expression& b,
                          const Expression& c, const Expression& d,
                          const Expression& e, const Expression& f,
                          const Expression& g) const {
        return (*this)({a, b, c, d, e, f, g});
    }

    [[nodiscard]] CustomFunctionHandle handle() const noexcept { return impl_; }

private:
    CustomFunctionHandle impl_;

    void ensure_valid() const {
        if (!impl_) {
            throw Nh::RuntimeException(
                "CustomFunction: invalid (null) handle");
        }
    }
};

// Factory function to create CUSTOM_FUNCTION Expression node
Expression make_custom_call(const CustomFunctionHandle& func,
                            const std::vector<Expression>& args);

#endif  // EXPRESSION__H
