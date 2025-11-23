// -*- c++ -*-
// Author: IWAI Jiro

#ifndef NH_COVARIANCE__H
#define NH_COVARIANCE__H

#include <map>
#include <memory>
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

    class CovarianceMatrix {
    public:
        CovarianceMatrix() : body_(std::make_shared<_CovarianceMatrix_>()) {}
        explicit CovarianceMatrix(std::shared_ptr<_CovarianceMatrix_> body)
            : body_(std::move(body)) {}

        _CovarianceMatrix_* operator->() const { return body_.get(); }
        _CovarianceMatrix_& operator*() const { return *body_; }
        [[nodiscard]] std::shared_ptr<_CovarianceMatrix_> shared() const { return body_; }

    private:
        std::shared_ptr<_CovarianceMatrix_> body_;
    };

    double covariance(const Normal& a, const Normal& b);
    double covariance(const RandomVariable& a, const RandomVariable& b);

    CovarianceMatrix& get_covariance_matrix();
}

#endif // NH_COVARIANCE__H
