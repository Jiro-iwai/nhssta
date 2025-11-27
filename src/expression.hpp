// -*- c++ -*-
// Author: Jiro Iwai

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

    // Inherit all constructors from HandleBase
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

    // assignment to (double)
    friend double& operator<<(double& a, const Expression& b);

    // print all expression infomation
    friend void print_all();

    // differential
    virtual Expression d(const Expression& y);

    enum Op { CONST = 0, PARAM, PLUS, MINUS, MUL, DIV, POWER, EXP, LOG };

    static void print_all();
    void print();

    ExpressionImpl();
    ExpressionImpl(double value);

    ExpressionImpl(const Op& op, const Expression& left, const Expression& right);

    virtual ~ExpressionImpl();

    [[nodiscard]] int id() const {
        return id_;
    }

   protected:
    [[nodiscard]] const Expression& left() const {
        return left_;
    }
    [[nodiscard]] const Expression& right() const {
        return right_;
    }

    virtual double value();
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
    using Dfrntls = std::map<Expression, Expression>;
    using Expressions = std::set<ExpressionImpl*>;

    int id_;
    bool is_set_value_;
    double value_;

    const Op op_;  // NOLINT(cppcoreguidelines-avoid-const-or-ref-data-members) - Op is a handle
                   // type, const is intentional
    Expression left_;
    Expression right_;
    Dfrntls dfrntls_;
    Expressions roots_;

    /// static data menber
    static int current_id_;
    static Expressions eTbl_;
};

// print all expression infomation
void print_all();

////////////////

class ConstImpl : public ExpressionImpl {
   public:
    ConstImpl(double value)
        : ExpressionImpl(value) {}
    ~ConstImpl() override = default;

   private:
    // differential
    Expression d(const Expression& y) override;
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
    Expression d(const Expression& y) override;
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

// assignment to (double)
double& operator<<(double& a, const Expression& b);

#endif  // EXPRESSION__H
