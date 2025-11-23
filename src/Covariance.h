// -*- c++ -*-
// Author: IWAI Jiro

#ifndef NH_COVARIANCE__H
#define NH_COVARIANCE__H

#include <map>
#include "RandomVariable.h"
#include "Normal.h"

namespace RandomVariable {

    class _CovarianceMatrix_ : public RCObject {
    public:

		using RowCol = std::pair<RandomVariable,RandomVariable>;
		using Matrix = std::map<RowCol,double>;

		_CovarianceMatrix_() = default;
		~_CovarianceMatrix_() override = default;

		bool lookup( const RandomVariable& a,
					 const RandomVariable& b, double& cov ) const;

		void set( const RandomVariable& a,
				  const RandomVariable& b, double cov ){
			RowCol rowcol(a,b);
			std::pair<RowCol,double> rowcolv(rowcol,cov);
			cmat.insert(rowcolv);
		}

    private:

		Matrix cmat;
    };

    class CovarianceMatrix : public SmartPtr<_CovarianceMatrix_> {
    public:
		CovarianceMatrix() : SmartPtr<_CovarianceMatrix_>
			( new _CovarianceMatrix_() ) {}
    };

    double covariance(const Normal& a, const Normal& b);
    double covariance(const RandomVariable& a, const RandomVariable& b);

    CovarianceMatrix& get_covariance_matrix();
}

#endif // NH_COVARIANCE__H
