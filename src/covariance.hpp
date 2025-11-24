// -*- c++ -*-
// Author: IWAI Jiro

#ifndef NH_COVARIANCE__H
#define NH_COVARIANCE__H

#include <unordered_map>
#include <memory>
#include <functional>
#include "random_variable.hpp"
#include "normal.hpp"

namespace RandomVariable {

    // Custom hash function for std::pair<RandomVariable, RandomVariable>
    // Uses the underlying shared_ptr addresses for hashing
    struct PairHash {
        template <class T1, class T2>
        std::size_t operator () (const std::pair<T1, T2>& p) const {
            // Hash based on the underlying shared_ptr addresses
            auto h1 = std::hash<const void*>{}(p.first.get());
            auto h2 = std::hash<const void*>{}(p.second.get());
            // Combine hashes
            return h1 ^ (h2 << 1);
        }
    };

    class _CovarianceMatrix_ {
    public:

		using RowCol = std::pair<RandomVariable,RandomVariable>;
		using Matrix = std::unordered_map<RowCol,double,PairHash>;

		_CovarianceMatrix_() = default;
		virtual ~_CovarianceMatrix_() = default;

		bool lookup( const RandomVariable& a,
					 const RandomVariable& b, double& cov ) const;

		void set( const RandomVariable& a,
				  const RandomVariable& b, double cov ){
			RowCol rowcol(a,b);
			cmat[rowcol] = cov;
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
