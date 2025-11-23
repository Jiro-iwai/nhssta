// -*- c++ -*-
// Author: IWAI Jiro

#include <cassert>
#include <cmath>
#include <typeinfo>
#include "Statistics.h"
#include "Util.h"

namespace RandomVariable {

    static CovarianceMatrix covariance_matrix;
    CovarianceMatrix& get_covariance_matrix() { return covariance_matrix; }

    bool _CovarianceMatrix_::lookup
    (
        const RandomVariable& a,
        const RandomVariable& b,
        double& cov
        ) const 
    {
        Matrix::const_iterator i;

        RowCol rowcol0(a,b);
        i = cmat.find(rowcol0);
        if( i != cmat.end() ) {
            cov = i->second;
            return true;
        }

        RowCol rowcol1(b,a);
        i = cmat.find(rowcol1);
        if( i != cmat.end() ) {
            cov = i->second;
            return true;
        }

        if( typeid(*a) == typeid(_Normal_) &&
            typeid(*b) == typeid(_Normal_) ){
            if( a == b ){
                cov = a->variance();
            } else {
                cov = 0.0;
            }
            return true;
        }
        return false;
    }

    static double covariance_x_max0_y
    (
        const RandomVariable& x, const RandomVariable& y
        )
    {
        assert( typeid(*y) == typeid(OpMAX0) );
        const RandomVariable& z = y->left();
        double c = covariance(x,z);
        double mu = z->mean();
        double vz = z->variance();
        assert( 0.0 < vz );
        double sz = sqrt(vz);
        double ms = -mu/sz;
        double mpx = MeanPhiMax(ms);
        double cov = c*mpx;
        assert( !std::isnan(cov) );
        return (cov);
    }

    static void check_covariance
    (
        double& cov,
        const RandomVariable& a,
        const RandomVariable& b
        )
    {
        double v0 = a->variance();
        double v1 = b->variance();
        double max_cov = sqrt(v0*v1);
        if( max_cov < minimum_variance ){
            assert( cov < minimum_variance );
            return;
        }
        double cor = cov / max_cov;
        double abs_cor = fabs(cor);
        if( 1.0 < abs_cor ) {
            double sig = cor/abs_cor;
            cov = sig * max_cov;
        }
    }

    double covariance(const Normal& a, const Normal& b)
    {
        double cov = 0.0;
        covariance_matrix->lookup(a,b,cov);
        return cov;
    }

    double covariance(const RandomVariable& a, const RandomVariable& b)
    {
        double cov = 0.0;

        if( !covariance_matrix->lookup(a,b,cov) ) {

            if( a == b ){
                cov = a->variance();

            } else if( typeid(*a) == typeid(OpADD)  ){
                double cov0 = covariance(a->left(),b);
                double cov1 = covariance(a->right(),b);
                cov = cov0 + cov1;

            } else if( typeid(*b) == typeid(OpADD)  ){
                double cov0 = covariance(a,b->left());
                double cov1 = covariance(a,b->right());
                cov = cov0 + cov1;

            } else if( typeid(*a) == typeid(OpSUB)  ){
                double cov0 = covariance(a->left(),b);
                double cov1 = covariance(a->right(),b);
                cov = cov0 - cov1;

            } else if( typeid(*b) == typeid(OpSUB)  ){
                double cov0 = covariance(a,b->left());
                double cov1 = covariance(a,b->right());
                cov = cov0 - cov1;

            } else if( typeid(*a) == typeid(OpMAX) ){
                const RandomVariable& x = a->left();
                const SmartPtr<OpMAX>& m(a);
                const RandomVariable& z = m->max0();
                double cov0 = covariance(z,b);
                double cov1 = covariance(x,b);
                cov = cov0 + cov1;

            } else if( typeid(*b) == typeid(OpMAX) ){
                const RandomVariable& x = b->left();
                const SmartPtr<OpMAX>& m(b);
                const RandomVariable& z = m->max0();
                double cov0 = covariance(z,a);
                double cov1 = covariance(x,a);
                cov = cov0 + cov1;

            } else if( typeid(*a) == typeid(OpMAX0) &&
                       typeid(*(a->left())) == typeid(OpMAX0) ){
                cov = covariance(a->left(),b);

            } else if( typeid(*b) == typeid(OpMAX0) &&
                       typeid(*(b->left())) == typeid(OpMAX0) ){
                cov = covariance(a,b->left());

            } else if( typeid(*a) == typeid(OpMAX0) &&
                       typeid(*b) == typeid(OpMAX0) ){
                if( a->left() == b->left() ){
                    cov = a->variance(); // maybe here is not reachable

                } else if( a->level() < b->level() ){
                    cov = covariance_x_max0_y(a,b);

                } else if( b->level() < a->level() ) {
                    cov = covariance_x_max0_y(b,a);

                } else {
                    double cov0 = covariance_x_max0_y(a,b);
                    double cov1 = covariance_x_max0_y(b,a);
                    cov = ( cov0 + cov1 ) * 0.5;
                }

            } else if( typeid(*a) == typeid(OpMAX0) ){
                cov = covariance_x_max0_y(b,a);

            } else if( typeid(*b) == typeid(OpMAX0) ){
                cov = covariance_x_max0_y(a,b);

            } else if( typeid(*a) == typeid(_Normal_) &&
                       typeid(*b) == typeid(_Normal_) ){
                cov = 0.0;

            } else {
                assert(0);

            }

            check_covariance(cov,a,b);
            covariance_matrix->set(a,b,cov);
        }

        return cov;
    }
}
