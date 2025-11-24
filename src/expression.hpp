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

////////////////////


class Expression_;

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
class ExpressionHandle {
   public:
    using element_type = Expression_;

    ExpressionHandle() = default;
    ExpressionHandle(std::nullptr_t)
        : body_(nullptr) {}

    // Takes ownership of the raw pointer (creates new shared_ptr)
    explicit ExpressionHandle(Expression_* body)
        : body_(body != nullptr ? std::shared_ptr<Expression_>(body) : nullptr) {}

    // Takes ownership via move (transfers shared_ptr ownership)
    explicit ExpressionHandle(std::shared_ptr<Expression_> body)
        : body_(std::move(body)) {}

    // Takes ownership via copy (shares shared_ptr ownership)
    template <class Derived, class = std::enable_if_t<std::is_base_of_v<Expression_, Derived>>>
    explicit ExpressionHandle(const std::shared_ptr<Derived>& body)
        : body_(std::static_pointer_cast<Expression_>(body)) {}

    // Non-owning access: returns raw pointer (no ownership transfer)
    Expression_* operator->() const {
        return body_.get();
    }
    Expression_& operator*() const {
        return *body_;
    }
    [[nodiscard]] Expression_* get() const {
        return body_.get();
    }

    // Ownership access: returns shared_ptr (creates new shared_ptr copy, shares ownership)
    [[nodiscard]] std::shared_ptr<Expression_> shared() const {
        return body_;
    }
    explicit operator bool() const {
        return static_cast<bool>(body_);
    }

    bool operator==(const ExpressionHandle& rhs) const {
        return body_.get() == rhs.body_.get();
    }
    bool operator!=(const ExpressionHandle& rhs) const {
        return !(*this == rhs);
    }
    bool operator<(const ExpressionHandle& rhs) const {
        return body_.get() < rhs.body_.get();
    }
    bool operator>(const ExpressionHandle& rhs) const {
        return body_.get() > rhs.body_.get();
    }

    template <class U>
    std::shared_ptr<U> dynamic_pointer_cast() const {
        auto ptr = std::dynamic_pointer_cast<U>(body_);
        if (!ptr) {
            throw Nh::RuntimeException("Expression: failed to dynamic cast");
        }
        return ptr;
    }

   private:
    // Owned shared_ptr: this Handle owns the underlying object
    // Copying the Handle shares this ownership (lightweight copy)
    std::shared_ptr<Expression_> body_;
};

// Type alias: Expression is a Handle (thin wrapper around std::shared_ptr)
// Usage:
// - Pass by value when ownership should be shared: void func(Expression expr)
// - Pass by const reference for read-only access: void func(const Expression& expr)
// - Store as member variable to create ownership: Expression left_;
using Expression = ExpressionHandle;

class Expression_ : public std::enable_shared_from_this<Expression_> {
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

    Expression_();
    Expression_(double value);

    Expression_(const Op& op, const Expression& left, const Expression& right);

    virtual ~Expression_();

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

    void add_root(Expression_* root);
    void remove_root(Expression_* root);

   protected:
    typedef std::map<Expression, Expression> Dfrntls;
    typedef std::set<Expression_*> Expressions;

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

class Const_ : public Expression_ {
   public:
    Const_(double value)
        : Expression_(value) {}
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
