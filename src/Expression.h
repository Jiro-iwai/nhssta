// -*- c++ -*-
// Author: Jiro Iwai

#ifndef EXPRESSION__H
#define EXPRESSION__H

#include <cassert>
#include <map>
#include <set>
#include "SmartPtr.h"

////////////////////

class ExpressionException {
public:
    ExpressionException(const std::string& what): what_(what) {
		assert(0);
    }
    const std::string& what() { return what_; }
private:
    std::string what_ ;
};

class Expression_;

// Expression is differentialable type.
typedef SmartPtr<Expression_> Expression;

class Expression_ : public RCObject {
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

    [[nodiscard]] int id() const { return id_; }

protected:

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

    virtual ~Expression_();

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
    virtual ~Const_() {}
private:
    // differential
    virtual Expression d(const Expression& y);
};

class Const : public SmartPtr<Const_> {
public:
    Const(double value);
    virtual ~Const() {}
};

////////////////

class Variable_ : public Expression_ {
public:
    Variable_() = default;
    ~Variable_() override = default;
    double value() override;
    Expression d(const Expression& y) override;
};

class Variable : public SmartPtr<Variable_> {
public:
    Variable();
    virtual ~Variable() {}

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
