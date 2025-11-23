// -*- c++ -*-
// Author: IWAI Jiro

#include <cassert>
#include <cmath>
#include <memory>
#include "Statistics.h"
#include "Util.h"
#include "ADD.h"
#include "SUB.h"
#include "MAX.h"

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

        const _Normal_* a_normal = dynamic_cast<const _Normal_*>(a.get());
        const _Normal_* b_normal = dynamic_cast<const _Normal_*>(b.get());
        if( a_normal && b_normal ){
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
        const OpMAX0* y_max0 = dynamic_cast<const OpMAX0*>(y.get());
        assert( y_max0 != nullptr );
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

            } else if( dynamic_cast<const OpADD*>(a.get()) != nullptr ){
                double cov0 = covariance(a->left(),b);
                double cov1 = covariance(a->right(),b);
                cov = cov0 + cov1;

            } else if( dynamic_cast<const OpADD*>(b.get()) != nullptr ){
                double cov0 = covariance(a,b->left());
                double cov1 = covariance(a,b->right());
                cov = cov0 + cov1;

            } else if( dynamic_cast<const OpSUB*>(a.get()) != nullptr ){
                double cov0 = covariance(a->left(),b);
                double cov1 = covariance(a->right(),b);
                cov = cov0 - cov1;

            } else if( dynamic_cast<const OpSUB*>(b.get()) != nullptr ){
                double cov0 = covariance(a,b->left());
                double cov1 = covariance(a,b->right());
                cov = cov0 - cov1;

            } else if( dynamic_cast<const OpMAX*>(a.get()) != nullptr ){
                const RandomVariable& x = a->left();
                auto m = a.dynamic_pointer_cast<const OpMAX>();
                const RandomVariable& z = m->max0();
                double cov0 = covariance(z,b);
                double cov1 = covariance(x,b);
                cov = cov0 + cov1;

            } else if( dynamic_cast<const OpMAX*>(b.get()) != nullptr ){
                const RandomVariable& x = b->left();
                auto m = b.dynamic_pointer_cast<const OpMAX>();
                const RandomVariable& z = m->max0();
                double cov0 = covariance(z,a);
                double cov1 = covariance(x,a);
                cov = cov0 + cov1;

            } else if( dynamic_cast<const OpMAX0*>(a.get()) != nullptr &&
                       dynamic_cast<const OpMAX0*>(a->left().get()) != nullptr ){
                cov = covariance(a->left(),b);

            } else if( dynamic_cast<const OpMAX0*>(b.get()) != nullptr &&
                       dynamic_cast<const OpMAX0*>(b->left().get()) != nullptr ){
                cov = covariance(a,b->left());

            } else if( dynamic_cast<const OpMAX0*>(a.get()) != nullptr &&
                       dynamic_cast<const OpMAX0*>(b.get()) != nullptr ){
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

            } else if( dynamic_cast<const OpMAX0*>(a.get()) != nullptr ){
                cov = covariance_x_max0_y(b,a);

            } else if( dynamic_cast<const OpMAX0*>(b.get()) != nullptr ){
                cov = covariance_x_max0_y(a,b);

            } else if( dynamic_cast<const _Normal_*>(a.get()) != nullptr &&
                       dynamic_cast<const _Normal_*>(b.get()) != nullptr ){
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
