// -*- c++ -*-
// Author: Jiro Iwai

#ifndef EXPRESSION__H
#define EXPRESSION__H

#include <cassert>
#include <map>
#include <set>
#include <memory>
#include <type_traits>
#include "SmartPtr.h"
#include "Exception.h"

////////////////////

// Backward compatibility: keep ExpressionException as alias to Nh::RuntimeException
// This will be removed in a later phase
// Note: ExpressionException originally had assert(0), but we remove it for unified exception handling
using ExpressionException = Nh::RuntimeException;

class Expression_;

class ExpressionHandle {
public:
    using element_type = Expression_;

    ExpressionHandle() = default;
    ExpressionHandle(std::nullptr_t) : body_(nullptr) {}
    explicit ExpressionHandle(Expression_* body)
        : body_(body != nullptr ? std::shared_ptr<Expression_>(body) : nullptr) {}
    explicit ExpressionHandle(std::shared_ptr<Expression_> body)
        : body_(std::move(body)) {}

    template <class Derived,
              class = std::enable_if_t<std::is_base_of_v<Expression_, Derived>>>
    explicit ExpressionHandle(const std::shared_ptr<Derived>& body)
        : body_(std::static_pointer_cast<Expression_>(body)) {}

    Expression_* operator->() const { return body_.get(); }
    Expression_& operator*() const { return *body_; }
    [[nodiscard]] Expression_* get() const { return body_.get(); }
    [[nodiscard]] std::shared_ptr<Expression_> shared() const { return body_; }
    explicit operator bool() const { return static_cast<bool>(body_); }

    bool operator==(const ExpressionHandle& rhs) const { return body_.get() == rhs.body_.get(); }
    bool operator!=(const ExpressionHandle& rhs) const { return !(*this == rhs); }
    bool operator<(const ExpressionHandle& rhs) const { return body_.get() < rhs.body_.get(); }
    bool operator>(const ExpressionHandle& rhs) const { return body_.get() > rhs.body_.get(); }

    template <class U>
    std::shared_ptr<U> dynamic_pointer_cast() const {
        auto ptr = std::dynamic_pointer_cast<U>(body_);
        if (!ptr) {
            throw Nh::RuntimeException("Expression: failed to dynamic cast");
        }
        return ptr;
    }

private:
    std::shared_ptr<Expression_> body_;
};

// Expression is differentialable type.
using Expression = ExpressionHandle;

class Expression_ : public RCObject, public std::enable_shared_from_this<Expression_> {
public:

    friend class Variable;

    friend Expression operator + (const Expression& a, const Expression& b);
    friend Expression operator + (double a, const Expression& b);
    friend Expression operator + (const Expression& a, double b);
    friend Expression operator + (const Expression& a);

    friend Expression operator - (const Expression& a, const Expression& b);
    friend Expression operator - (double a, const Expression& b);
    friend Expression operator - (const Expression& a, double b);
    friend Expression operator - (const Expression& a);

    friend Expression operator * (const Expression& a, const Expression& b);
    friend Expression operator * (double a, const Expression& b);
    friend Expression operator * (const Expression& a, double b);

    friend Expression operator / (const Expression& a, const Expression& b);
    friend Expression operator / (const Expression& a, double b);
    friend Expression operator / (double a, const Expression& b);

    friend Expression operator ^ (const Expression& a, const Expression& b);
    friend Expression operator ^ (const Expression& a, double b);
    friend Expression operator ^ (double a, const Expression& b);

    friend Expression exp (const Expression& a);
    friend Expression log (const Expression& a);

    // assignment to (double) 
    friend double& operator << (double& a, const Expression& b);

    // print all expression infomation
    friend void print_all();

    // differential
    virtual Expression d(const Expression& y);

    enum Op {
		CONST = 0,
		PARAM, 
		PLUS,
		MINUS,
		MUL,
		DIV,
		POWER,
		EXP,
		LOG
    };

    static void print_all();
    void print();

    Expression_();
    Expression_(double value);

    Expression_
    (
		const Op& op, 
		const Expression& left, 
		const Expression& right
		);

    ~Expression_();

    [[nodiscard]] int id() const { return id_; }

protected:

    [[nodiscard]] const Expression& left() const { return left_; }
    [[nodiscard]] const Expression& right() const { return right_; }

    virtual double value();
    void set_value(double value);
    void _set_value(double value);
    void unset_value();
    void unset_root_value();
    [[nodiscard]] bool is_set_value() const { return is_set_value_; }

    void add_root(Expression_* root);
    void remove_root(Expression_* root);

protected:

    typedef std::map<Expression,Expression> Dfrntls;
    typedef std::set<Expression_*> Expressions;

    int id_;
    bool is_set_value_;
    double value_;

    const Op op_;
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

class Const_ : public Expression_ {
public:
    Const_(double value) : Expression_(value) {}
    ~Const_() override = default;
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

class Variable_ : public Expression_ {
public:
    Variable_() = default;
    ~Variable_() override = default;
    double value() override;
    Expression d(const Expression& y) override;
};

class Variable : public Expression {
public:
    Variable();
    ~Variable() = default;

    ////// substitution from (double)
    Variable& operator = (double value);
};

////////////////

Expression operator + (const Expression& a, const Expression& b);
Expression operator + (double a, const Expression& b);
Expression operator + (const Expression& a, double b);
Expression operator + (const Expression& a);

Expression operator - (const Expression& a, const Expression& b);
Expression operator - (double a, const Expression& b);
Expression operator - (const Expression& a, double b);
Expression operator - (const Expression& a);

Expression operator * (const Expression& a, const Expression& b);
Expression operator * (double a, const Expression& b);
Expression operator * (const Expression& a, double b);

Expression operator / (const Expression& a, const Expression& b);
Expression operator / (const Expression& a, double b);
Expression operator / (double a, const Expression& b);

Expression operator ^ (const Expression& a, const Expression& b);
Expression operator ^ (const Expression& a, double b);
Expression operator ^ (double a, const Expression& b);

Expression exp (const Expression& a);
Expression log (const Expression& a);

// assignment to (double)
double& operator << (double& a, const Expression& b);

#endif // EXPRESSION__H
