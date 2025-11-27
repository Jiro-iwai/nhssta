// -*- c++ -*-
// Author: IWAI Jiro

#ifndef NH_COVARIANCE__H
#define NH_COVARIANCE__H

#include <functional>
#include <memory>
#include <unordered_map>

#include "normal.hpp"
#include "random_variable.hpp"

namespace RandomVariable {

// Custom hash function for std::pair<RandomVariable, RandomVariable>
// Uses the underlying shared_ptr addresses for hashing
struct PairHash {
    template <class T1, class T2>
    std::size_t operator()(const std::pair<T1, T2>& p) const {
        // Hash based on the underlying shared_ptr addresses
        auto h1 = std::hash<const void*>{}(p.first.get());
        auto h2 = std::hash<const void*>{}(p.second.get());
        // Combine hashes
        return h1 ^ (h2 << 1);
    }
};

class CovarianceMatrixImpl {
   public:
    using RowCol = std::pair<RandomVariable, RandomVariable>;
    using Matrix = std::unordered_map<RowCol, double, PairHash>;

    CovarianceMatrixImpl() = default;
    virtual ~CovarianceMatrixImpl() = default;

    bool lookup(const RandomVariable& a, const RandomVariable& b, double& cov) const;

    void set(const RandomVariable& a, const RandomVariable& b, double cov) {
        RowCol rowcol(a, b);
        cmat[rowcol] = cov;
    }

   private:
    Matrix cmat;
};

class CovarianceMatrix {
   public:
    CovarianceMatrix()
        : body_(std::make_shared<CovarianceMatrixImpl>()) {}
    explicit CovarianceMatrix(std::shared_ptr<CovarianceMatrixImpl> body)
        : body_(std::move(body)) {}

    CovarianceMatrixImpl* operator->() const {
        return body_.get();
    }
    CovarianceMatrixImpl& operator*() const {
        return *body_;
    }
    [[nodiscard]] std::shared_ptr<CovarianceMatrixImpl> shared() const {
        return body_;
    }

   private:
    std::shared_ptr<CovarianceMatrixImpl> body_;
};

double covariance(const Normal& a, const Normal& b);
double covariance(const RandomVariable& a, const RandomVariable& b);

CovarianceMatrix& get_covariance_matrix();
}  // namespace RandomVariable

#endif  // NH_COVARIANCE__H
