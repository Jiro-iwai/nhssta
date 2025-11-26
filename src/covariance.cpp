// -*- c++ -*-
// Author: IWAI Jiro

#include <cassert>
#include <cmath>
#include <memory>
#include <nhssta/exception.hpp>

#include "add.hpp"
#include "max.hpp"
#include "statistics.hpp"
#include "sub.hpp"
#include "util_numerical.hpp"

namespace RandomVariable {

static CovarianceMatrix covariance_matrix;
CovarianceMatrix& get_covariance_matrix() {
    return covariance_matrix;
}

bool _CovarianceMatrix_::lookup(const RandomVariable& a, const RandomVariable& b,
                                double& cov) const {
    RowCol rowcol0(a, b);
    auto i = cmat.find(rowcol0);
    if (i != cmat.end()) {
        cov = i->second;
        return true;
    }

    RowCol rowcol1(b, a);
    i = cmat.find(rowcol1);
    if (i != cmat.end()) {
        cov = i->second;
        return true;
    }

    const auto* a_normal = dynamic_cast<const _Normal_*>(a.get());
    const auto* b_normal = dynamic_cast<const _Normal_*>(b.get());
    if (a_normal != nullptr && b_normal != nullptr) {
        if (a == b) {
            cov = a->variance();
        } else {
            cov = 0.0;
        }
        return true;
    }
    return false;
}

// covariance_x_max0_y(x, y):
// y must be OpMAX0 (y = max(0, Z)), and (x, Z) must be joint Gaussian.
// Computes Cov(x, max(0, Z)) = Cov(x, Z) * Φ(-μ_Z/σ_Z)
// WARNING: Do not use when x is OpMAX or OpMAX0 (non-Gaussian).
static double covariance_x_max0_y(const RandomVariable& x, const RandomVariable& y) {
    const auto* y_max0 = dynamic_cast<const OpMAX0*>(y.get());
    if (y_max0 == nullptr) {
        throw Nh::RuntimeException("covariance_x_max0_y: y must be OpMAX0 type");
    }
    const RandomVariable& z = y->left();
    double c = covariance(x, z);
    double mu = z->mean();
    double vz = z->variance();
    if (vz <= 0.0) {
        throw Nh::RuntimeException("covariance_x_max0_y: variance must be positive, got " +
                                   std::to_string(vz));
    }
    double sz = sqrt(vz);
    double ms = -mu / sz;
    double mpx = MeanPhiMax(ms);
    double cov = c * mpx;
    if (std::isnan(cov)) {
        throw Nh::RuntimeException("covariance_x_max0_y: covariance calculation resulted in NaN");
    }
    return (cov);
}

// covariance_max0_max0(a, b):
// Both a and b must be OpMAX0: a = max(0, D0), b = max(0, D1)
// Uses 1D Gauss-Hermite quadrature to compute Cov(max(0,D0), max(0,D1))
// where D0, D1 are assumed to be jointly Gaussian.
static double covariance_max0_max0(const RandomVariable& a, const RandomVariable& b) {
    // Get the underlying random variables D0 and D1
    const RandomVariable& d0 = a->left();
    const RandomVariable& d1 = b->left();

    // Get statistics for D0
    double mu0 = d0->mean();
    double v0 = d0->variance();
    if (v0 <= 0.0) {
        throw Nh::RuntimeException("covariance_max0_max0: D0 variance must be positive");
    }
    double sigma0 = sqrt(v0);

    // Get statistics for D1
    double mu1 = d1->mean();
    double v1 = d1->variance();
    if (v1 <= 0.0) {
        throw Nh::RuntimeException("covariance_max0_max0: D1 variance must be positive");
    }
    double sigma1 = sqrt(v1);

    // Get covariance between D0 and D1 (recursive call)
    double covD = covariance(d0, d1);

    // Compute correlation coefficient ρ = Cov(D0,D1) / (σ0 * σ1)
    // Clamp to [-1, 1] for numerical stability
    double rho = covD / (sigma0 * sigma1);
    if (rho > 1.0) rho = 1.0;
    if (rho < -1.0) rho = -1.0;

    // E[D0⁺] and E[D1⁺]
    double E0pos = expected_positive_part(mu0, sigma0);
    double E1pos = expected_positive_part(mu1, sigma1);

    // E[D0⁺ D1⁺] via Gauss-Hermite quadrature
    double Eprod = expected_prod_pos(mu0, sigma0, mu1, sigma1, rho);

    // Cov(D0⁺, D1⁺) = E[D0⁺ D1⁺] - E[D0⁺] * E[D1⁺]
    double cov = Eprod - E0pos * E1pos;

    if (std::isnan(cov)) {
        throw Nh::RuntimeException("covariance_max0_max0: result is NaN");
    }

    return cov;
}

static void check_covariance(double& cov, const RandomVariable& a, const RandomVariable& b) {
    double v0 = a->variance();
    double v1 = b->variance();
    double max_cov = sqrt(v0 * v1);
    if (max_cov < MINIMUM_VARIANCE) {
        if (cov >= MINIMUM_VARIANCE) {
            throw Nh::RuntimeException("check_covariance: covariance " + std::to_string(cov) +
                                       " exceeds maximum possible value " +
                                       std::to_string(MINIMUM_VARIANCE));
        }
        return;
    }
    double cor = cov / max_cov;
    double abs_cor = fabs(cor);
    if (1.0 < abs_cor) {
        double sig = cor / abs_cor;
        cov = sig * max_cov;
    }
}

double covariance(const Normal& a, const Normal& b) {
    double cov = 0.0;
    covariance_matrix->lookup(a, b, cov);
    return cov;
}

double covariance(const RandomVariable& a, const RandomVariable& b) {
    double cov = 0.0;

    if (!covariance_matrix->lookup(a, b, cov)) {
        if (a == b) {
            cov = a->variance();

        } else if (dynamic_cast<const OpADD*>(a.get()) != nullptr) {
            double cov0 = covariance(a->left(), b);
            double cov1 = covariance(a->right(), b);
            cov = cov0 + cov1;

        } else if (dynamic_cast<const OpADD*>(b.get()) != nullptr) {
            double cov0 = covariance(a, b->left());
            double cov1 = covariance(a, b->right());
            cov = cov0 + cov1;

        } else if (dynamic_cast<const OpSUB*>(a.get()) != nullptr) {
            double cov0 = covariance(a->left(), b);
            double cov1 = covariance(a->right(), b);
            cov = cov0 - cov1;

        } else if (dynamic_cast<const OpSUB*>(b.get()) != nullptr) {
            double cov0 = covariance(a, b->left());
            double cov1 = covariance(a, b->right());
            cov = cov0 - cov1;

        } else if (dynamic_cast<const OpMAX*>(a.get()) != nullptr) {
            const RandomVariable& x = a->left();
            auto m = a.dynamic_pointer_cast<const OpMAX>();
            const RandomVariable& z = m->max0();
            double cov0 = covariance(z, b);
            double cov1 = covariance(x, b);
            cov = cov0 + cov1;

        } else if (dynamic_cast<const OpMAX*>(b.get()) != nullptr) {
            const RandomVariable& x = b->left();
            auto m = b.dynamic_pointer_cast<const OpMAX>();
            const RandomVariable& z = m->max0();
            double cov0 = covariance(z, a);
            double cov1 = covariance(x, a);
            cov = cov0 + cov1;

        } else if (dynamic_cast<const OpMAX0*>(a.get()) != nullptr &&
                   dynamic_cast<const OpMAX0*>(a->left().get()) != nullptr) {
            cov = covariance(a->left(), b);

        } else if (dynamic_cast<const OpMAX0*>(b.get()) != nullptr &&
                   dynamic_cast<const OpMAX0*>(b->left().get()) != nullptr) {
            cov = covariance(a, b->left());

        } else if (dynamic_cast<const OpMAX0*>(a.get()) != nullptr &&
                   dynamic_cast<const OpMAX0*>(b.get()) != nullptr) {
            // Both a and b are OpMAX0: use dedicated covariance_max0_max0
            // which uses Gauss-Hermite quadrature for correct bivariate normal handling
            if (a->left() == b->left()) {
                // max(0,D) with itself: Cov = Var(max(0,D))
                cov = a->variance();
            } else {
                cov = covariance_max0_max0(a, b);
            }

        } else if (dynamic_cast<const OpMAX0*>(a.get()) != nullptr) {
            cov = covariance_x_max0_y(b, a);

        } else if (dynamic_cast<const OpMAX0*>(b.get()) != nullptr) {
            cov = covariance_x_max0_y(a, b);

        } else if (dynamic_cast<const _Normal_*>(a.get()) != nullptr &&
                   dynamic_cast<const _Normal_*>(b.get()) != nullptr) {
            cov = 0.0;

        } else {
            throw Nh::RuntimeException(
                "covariance: unsupported RandomVariable type combination for covariance "
                "calculation");
        }

        check_covariance(cov, a, b);
        covariance_matrix->set(a, b, cov);
    }

    return cov;
}
}  // namespace RandomVariable
