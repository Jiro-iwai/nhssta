// -*- c++ -*-
// Author: Jiro Iwai

#include <iostream>
#include <cmath>
#include <algorithm>
#include <functional>
#include <boost/format.hpp>
#include "Expression.h"

int Expression_::current_id_ = 0;
Expression_::Expressions Expression_::eTbl_;

static Expression null(0);
static const Const zero(0.0);
static const Const one(1.0);
static const Const minus_one(-1.0);

Expression_::Expression_() :
	id_(current_id_++),
	is_set_value_(false),
	value_(0.0),
	op_(PARAM),
	left_(null),
	right_(null)
{
	eTbl_.insert(this);
}

Expression_::Expression_(double value) :
	id_(current_id_++),
	is_set_value_(true),
	value_(value),
	op_(CONST),
	left_(null),
	right_(null)
{
	eTbl_.insert(this);
}

Expression_::Expression_
(
	const Op& op, 
	const Expression& left, 
	const Expression& right
	) :
	id_(current_id_++),
	is_set_value_(false),
	value_(0.0),
	op_(op),
	left_(left),
	right_(right)
{
	eTbl_.insert(this);
	if( left_ != null ){ left_->add_root(this); }
	if( right_ != null ){ right_->add_root(this); }
}

Expression_::~Expression_() { 
	eTbl_.erase(this);
	if( left() != null ){ left()->remove_root(this); }
	if( right() != null ){ right()->remove_root(this); }
}

double Expression_::value() { 
	if( !is_set_value() ) {
		if( left() != null && right() != null ) {

			double l = left()->value();
			double r = right()->value();

			if( op_ == PLUS ) {
				value_ = l + r;
			} else if( op_ == MINUS ) {
				value_ = l - r;
			} else if( op_ == MUL ) {
				value_ = l * r;
			} else if( op_ == DIV ) {
				if( r == 0.0 ){
					throw ExpressionException("");
				}
				value_ = l / r;
			} else if( op_ == POWER ) {
				value_ = std::pow(l,r);
			} else {
				throw ExpressionException("");
			}

		} else if( left() != null ) {

			double l = left()->value();

			if( op_ == EXP ) {
				value_ = std::exp(l);
			} else if( op_ == LOG ) {
				if(l < 0.0 ) {
					throw ExpressionException("");
				}
				value_ = std::log(l);
			}

		} else {
			throw ExpressionException("");
		}
		is_set_value_ = true;
	}
	return (value_); 
}

Expression Expression_::d(const Expression& y) {

	Expression x(this);
	Expression dx;

	auto i = dfrntls_.find(y);
	if( i != dfrntls_.end() ) {
		dx = i->second;
		return dx;
	}
	if( x == y ) {
		dx = one;
		return dx;

	} else if( left() != null && right() != null ) {

		Expression dl = left()->d(y);
		Expression dr = right()->d(y);

		if ( op_ == PLUS ) {
			dx = dl + dr;
		} else if( op_ == MINUS ) {
			dx = dl - dr;
		} else if( op_ == MUL ) {
			dx = dl*right() + left()*dr;
		} else if( op_ == DIV ) {
			dx = ( dl - x*dr )/right();
		} else if( op_ == POWER ) {
			dx = x * ( dl/left()*right() + dr*log(left()) );
		} else {
			throw ExpressionException("");
		}

	} else if( left() != null ) {

		Expression dl = left()->d(y);
		    
		if( op_ == EXP ) {
			dx = x * dl;
		} else if( op_ == LOG ) {
			dx = dl / left();
		}
	}

	dfrntls_[y] = dx;
	return dx;
}

void Expression_::set_value(double value) { 
	unset_root_value();
	_set_value(value);
}

void Expression_::_set_value(double value) { 
	value_ = value;
	is_set_value_ = true;
}

void Expression_::unset_value() { 
	if( !is_set_value() )
		return;

	unset_root_value();
	value_ = 0.0;
	is_set_value_ = false;
}

void Expression_::unset_root_value() { 
        for_each(roots_.begin(),roots_.end(),std::mem_fn(&Expression_::unset_value));
}

void Expression_::add_root(Expression_* root){
	roots_.insert(root);
}

void Expression_::remove_root(Expression_* root){
	roots_.erase(root);
}

void Expression_::print () {

	std::cout << boost::format("%5d") % id_;

	if( is_set_value() ) {
		std::cout << boost::format("%10.4f") % value();
	} else {
		std::cout << boost::format("%10s") % "--";
	}

	if( op_ == CONST ) {
		std::cout << boost::format("%10s") % "CONST";
	} else if( op_ == PARAM ) {
		std::cout << boost::format("%10s") % "PARAM";
	} else if( op_ == PLUS ) {
		std::cout << boost::format("%10s") % "+";
	} else if( op_ == MINUS) {
		std::cout << boost::format("%10s") % "-";
	} else if( op_ == MUL ) {
		std::cout << boost::format("%10s") % "x";
	} else if( op_ == DIV ) {
		std::cout << boost::format("%10s") % "/";
	} else if( op_ == EXP ) {
		std::cout << boost::format("%10s") % "exp";
	} else if( op_ == LOG ) {
		std::cout << boost::format("%10s") % "log";
	} else if( op_ == POWER ) {
		std::cout << boost::format("%10s") % "^";
	}

	if( left() != null  ){
		if( left()->is_set_value() )
			std::cout << boost::format("%10.4f") % left()->value();
		else 
			std::cout << boost::format("%10s") % "--";
	} else {
		std::cout << boost::format("%10s") % "--";
	}

	if( right() != null	) {
		if( right()->is_set_value() )
			std::cout << boost::format("%10.4f") % right()->value();
		else 
			std::cout << boost::format("%10s") % "--";
	} else {
		std::cout << boost::format("%10s") % "--";
	}


	if( left() != null  ){
		std::cout << boost::format("%10d") % left()->id();
	} else {
		std::cout << boost::format("%10s") % "--";
	}

	if( right() != null	) {
		std::cout << boost::format("%10d") % right()->id();
	} else {
		std::cout << boost::format("%10s") % "--";
	}

	std::cout << std::endl;
}

void Expression_::print_all() {
        for_each(eTbl_.begin(),eTbl_.end(),std::mem_fn(&Expression_::print));
}

void print_all(){
	Expression_::print_all();
}

//////////////////////////

Const::Const(double value) : SmartPtr<Const_>(new Const_(value)) {}

Expression Const_::d(const Expression& y) { return zero; }

//////////////////////////

Expression Variable_::d(const Expression& y) {
	if( Expression(this) == y ){
		return one;
	} else {
		return zero;
	}
}

double Variable_::value() { 
	if( !is_set_value() ) {
		throw ExpressionException("");
	}
	return (value_); 
}

Variable::Variable() : SmartPtr<Variable_>(new Variable_()) {}

void Variable::operator = (double value) { (*this)->set_value(value); } 

//////////////////////////

Expression operator + (const Expression& a, const Expression& b){
	if( a == zero ) {
		return b;
	} else if( b == zero ) {
		return a;
	}
	return Expression( new Expression_(Expression_::PLUS, a, b) );
}

Expression operator + (double a, const Expression& b){
	return ( Const(a) + b );
}

Expression operator + (const Expression& a, double b){
	return (b + a);
}

Expression operator + (const Expression& a){
	if( a == zero ){
		return zero;
	} 
	return (a);
}

//////////////////////////

Expression operator - (const Expression& a, const Expression& b){
	if( a == zero ) {
		return (-b);
	} else if( b == zero ) {
		return a;
	} 
	return Expression( new Expression_(Expression_::MINUS, a, b) );
}

Expression operator - (double a, const Expression& b){
	return ( Const(a) - b );
}

Expression operator - (const Expression& a, double b){
	return ( (-b) + a );
}

Expression operator - (const Expression& a){
	if( a == zero ){
		return zero;
	} else if( a == minus_one ) {
		return one;
	} 
	return Expression( new Expression_(Expression_::MUL, minus_one, a) );
}

//////////////////////////

Expression operator * (const Expression& a, const Expression& b){
	if( a == zero || b == zero ) {
		return zero;
	} else if( a == one ) {
		return b;
	} else if( b == one ) {
		return a;
	} 
	return Expression( new Expression_(Expression_::MUL, a, b) );
}

Expression operator * (double a, const Expression& b){
	return ( Const(a) * b );
}

Expression operator * (const Expression& a, double b){
	return ( b * a );
}

//////////////////////////

Expression operator / (const Expression& a, const Expression& b){
	if( b == zero ) {
		throw ExpressionException("");
	} else if( a == zero ) {
		return zero;
	} else if( b == one ) {
		return a;
	} else if( b == minus_one ) {
		return (-a);
	} else if( a == b ) {
		return one;
	}
	return Expression( new Expression_(Expression_::DIV, a, b) );
}

Expression operator / (const Expression& a, double b){
	return ( a / Const(b) );
}

Expression operator / (double a, const Expression& b){
	return ( Const(a) / b );
}

//////////////////////////

Expression operator ^ (const Expression& a, const Expression& b){
	if( b == zero ) {
		if( a == zero )
			throw ExpressionException("");
		else 
			return one;
	} else if( b == one ) {
		return a;
	} else if( a == zero ) {
		return zero;
	}
	return Expression( new Expression_(Expression_::POWER, a, b) );
}

Expression operator ^ (const Expression& a, double b){
	return ( a ^ Const(b) );
}

Expression operator ^ (double a, const Expression& b){
	return ( Const(a) ^ b );
}


//////////////////////////

Expression exp (const Expression& a){
	return Expression( new Expression_(Expression_::EXP, a, null) );
}

Expression log (const Expression& a){
	return Expression( new Expression_(Expression_::LOG, a, null) );
}

//////////////////////////

// assignment to (double) 
double& operator << (double& a, const Expression& b) {
	a = b->value();
	return a;
}

//////////////////////////
