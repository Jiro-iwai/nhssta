// -*- c++ -*-
// Author: IWAI Jiro

#include <cmath>
#include <iostream>
#include <vector>
#include "expression.hpp"

namespace Nh {

    // ³ÎÎ¨ÊÑ¿ô¤Î´ğËÜ±é»»¤Î¥Æ¥¹¥È
    void test0() {

	Variable x, y;
	Expression z;

	double v;

	///////////

	z = 2.0*x;

	x = 1.0;

	assert( (v << z) == 2.0 );
	assert( (v << z->d(x)) == 2.0 );
	assert( (v << z->d(y)) == 0.0 );

	x = 2.0;

	assert( (v << z) == 4.0 );
	assert( (v << z->d(x)) == 2.0 );
	assert( (v << z->d(y)) == 0.0 );

	//////////

	z = x + y;

	x = 1.0;
	y = 1.0;

	assert( (v << z) == 2.0 );
	assert( (v << z->d(x)) == 1.0 );
	assert( (v << z->d(y)) == 1.0 );

	x = 2.0;
	y = -1.0;

	assert( (v << z) == 1.0 );
	assert( (v << z->d(x)) == 1.0 );
	assert( (v << z->d(y)) == 1.0 );

	//////////

	z = x*y;

	x = 3.0;
	y = 4.0;

	assert( (v << z) == 12.0 );
	assert( (v << z->d(x)) == 4.0 );
	assert( (v << z->d(y)) == 3.0 );

	x = -1.0;
	y = 2.0;

	assert( (v << z ) == -2.0 );
	assert( (v << z->d(x)) == 2.0 );
	assert( (v << z->d(y)) == -1.0 );

	////////

	z = x - y;
	x = 2.0;
	y = 1.0;

	assert( (v << z ) == 1.0 );
	assert( (v << z->d(x)) == 1.0 );
	assert( (v << z->d(y)) == -1.0 );


	//////

	z = x/y;
	x = 2.0;
	y = 1.0;

	assert( (v << z ) == 2.0 );
	assert( (v << z->d(x)) == 1.0 );
	assert( (v << z->d(y)) == -2.0 );


	//////

	Variable w;

	z = x + y + w;
	x = 2.0;
	y = 1.0;
	w = 1.0;

	assert( (v << z ) == 4.0 );
	assert( (v << z->d(x)) == 1.0 );
	assert( (v << z->d(y)) == 1.0 );
	assert( (v << z->d(w)) == 1.0 );


	//////

	z = x*y*w;
	x = 2.0;
	y = 1.0;
	w = 1.0;

	assert( (v << z) == 2.0 );
	assert( (v << z->d(x)) == 1.0 );
	assert( (v << z->d(y)) == 2.0 );
	assert( (v << z->d(w)) == 2.0 );

	//////
	
	z = exp(x);
	x = 0.0;
	assert( (v << x) == 0.0 );
	assert( (v << z) == 1.0 );
	assert( (v << z->d(x)) == 1.0 );


	//////
	
	z = log(x*x*y/w*w)*exp(x)/w;
	x = 1.0;
// 	assert( (v << x) == 1.0 );
// 	assert( (v << z) == 0.0 );
// 	assert( (v << z->d(x)) == 1.0 );

	v << z->d(x);
	v << z->d(y);
	v << z->d(w);

	////

	z = x^2;
	x = 2.0;

 	assert( (v << x) == 2.0 );
 	assert( (v << z) == 4.0 );
 	assert( (v << z->d(x)) == 4.0 );

	print_all();
    }


//     // INVERTER ¤Î ÃÙ±ä·×»»
//     void test1(){

// 	Gate g0;
// 	g0->set_type_name("INV");

// 	Paramater d(1.0,1.0);
// 	g0->set_delay("IN","OUT",d);

// 	////

// 	Instance i0 = g0->create_instance();

// 	Paramater in(0.0,1.0);
// 	i0->set_input("IN",in);

// 	const RandomVariable& out = i0->output("OUT");
// 	std::cout << "mean of delay: " << out->mean() << std::endl;
// 	std::cout << "variance of delay: " << out->variance() << std::endl;;
//     }


//     // AND ¤Î ÃÙ±ä·×»»
//     void test2() {

// 	Gate g0;
// 	g0->set_type_name("AND");

// 	Paramater d0(1.0,1.0);
// 	g0->set_delay("IN0","OUT",d0);

// 	Paramater d1(1.0,1.0);
// 	g0->set_delay("IN1","OUT",d1);

// 	////

// 	Instance i0 = g0->create_instance();

// 	Paramater in0(1.0,1.0);
// 	i0->set_input("IN0",in0);

// 	Paramater in1(1.0,1.0);
// 	i0->set_input("IN1",in1);


// 	Instance i1 = g0->create_instance();

// 	Paramater in2(1.0,1.0);
// 	i1->set_input("IN0",in2);

// 	Paramater in3(1.0,1.0);
// 	i1->set_input("IN1",in3);


// 	const RandomVariable& out0 = i0->output("OUT");
// 	const RandomVariable& out1 = i1->output("OUT");


// 	Instance i2 = g0->create_instance();
// 	i2->set_input("IN0",out0);
// 	i2->set_input("IN1",out1);


// //	const RandomVariable& out2 = i2->output("OUT");
// //	std::cout << "mean of delay: " << out2->mean() << std::endl;
// //	std::cout << "variance of delya: " << out2->variance() << std::endl;;

// 	const RandomVariable& out2 = i2->output("OUT");
// 	std::cout << "cov(2,1)=" << covariance(out2,out1) << std::endl;
// 	std::cout << "cov(2,0)=" << covariance(out2,out0) << std::endl;


//     }
}

int main(int argc, char *argv[]) {

    Nh::test0();
    //Nh::test1();
    //Nh::test2();
    return(0);
}

