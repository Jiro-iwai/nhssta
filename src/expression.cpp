// -*- c++ -*-
// Author: Jiro Iwai

#include "expression.hpp"
#include "util_numerical.hpp"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <functional>
#include <iomanip>
#include <iostream>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <vector>

// Local helper functions for bivariate normal distribution
namespace {

// Use normal_pdf and normal_cdf from util_numerical.hpp
using RandomVariable::normal_pdf;
using RandomVariable::normal_cdf;

// Bivariate normal PDF: φ₂(x, y; ρ)
double compute_phi2(double x, double y, double rho) {
    if (std::abs(rho) > 0.9999) {
        // Degenerate case - return a small value
        return 0.0;
    }
    double one_minus_rho2 = 1.0 - (rho * rho);
    double coeff = 1.0 / (2.0 * M_PI * std::sqrt(one_minus_rho2));
    double Q = (x * x - 2.0 * rho * x * y + y * y) / one_minus_rho2;
    return coeff * std::exp(-Q / 2.0);
}

// Bivariate normal CDF: Φ₂(h, k; ρ) using Simpson's rule
// 128 points provides 8-digit accuracy with good performance (~1.6μs)
double compute_Phi2(double h, double k, double rho, int n_points = 128) {
    // Handle edge cases
    if (std::abs(rho) > 0.9999) {
        if (rho > 0) {
            return normal_cdf(std::min(h, k));
        }
        return std::max(0.0, normal_cdf(h) + normal_cdf(k) - 1.0);
    }
    if (std::abs(rho) < 1e-10) {
        return normal_cdf(h) * normal_cdf(k);
    }

    // Integrate φ(x) × Φ((k - ρx)/σ') from -∞ to h
    double sigma_prime = std::sqrt(1.0 - (rho * rho));
    double lower_bound = -8.0;  // Effectively -∞ for standard normal
    double upper_bound = h;

    if (upper_bound < lower_bound) {
        return 0.0;
    }

    double sum = 0.0;
    double dx = (upper_bound - lower_bound) / n_points;

    for (int i = 0; i <= n_points; ++i) {
        double x = lower_bound + (i * dx);
        double f_val = normal_pdf(x) * normal_cdf((k - rho * x) / sigma_prime);
        // Simpson's rule weights: 1-4-2-4-2-...-4-1
        double weight = 1.0;
        if (i != 0 && i != n_points) {
            weight = (i % 2 == 0) ? 2.0 : 4.0;
        }
        sum += weight * f_val;
    }
    return sum * dx / 3.0;
}

}  // anonymous namespace

int ExpressionImpl::current_id_ = 0;

// Flag to track if eTbl has been destroyed
static bool eTbl_destroyed = false;

// Function-local static to avoid static destruction order issues
// This ensures eTbl is not destroyed while ExpressionImpl objects still exist
ExpressionImpl::Expressions& ExpressionImpl::eTbl() {
    static Expressions tbl;
    static struct Guard {
        ~Guard() { eTbl_destroyed = true; }
    } guard;
    return tbl;
}

static Expression null(nullptr);
static const Const zero(0.0);
static const Const one(1.0);
static const Const minus_one(-1.0);
static const Const two(2.0);
static const Const half(0.5);

ExpressionImpl::ExpressionImpl()
    : id_(current_id_++)
    , is_set_value_(false)
    , value_(0.0)
    , gradient_(0.0)
    , is_gradient_set_(false)
    , op_(PARAM)
    , left_(null)
    , right_(null) {
    eTbl().insert(this);
}

ExpressionImpl::ExpressionImpl(double value)
    : id_(current_id_++)
    , is_set_value_(true)
    , value_(value)
    , gradient_(0.0)
    , is_gradient_set_(false)
    , op_(CONST)
    , left_(null)
    , right_(null) {
    eTbl().insert(this);
}

ExpressionImpl::ExpressionImpl(const Op& op, const Expression& left, const Expression& right)
    : id_(current_id_++)
    , is_set_value_(false)
    , value_(0.0)
    , gradient_(0.0)
    , is_gradient_set_(false)
    , op_(op)
    , left_(left)
    , right_(right)
    , third_(null) {
    eTbl().insert(this);
    if (left_ != null) {
        left_->add_root(this);
    }
    if (right_ != null) {
        right_->add_root(this);
    }
}

ExpressionImpl::ExpressionImpl(const Op& op, const Expression& first, const Expression& second,
                               const Expression& third)
    : id_(current_id_++)
    , is_set_value_(false)
    , value_(0.0)
    , gradient_(0.0)
    , is_gradient_set_(false)
    , op_(op)
    , left_(first)
    , right_(second)
    , third_(third) {
    eTbl().insert(this);
    if (left_ != null) {
        left_->add_root(this);
    }
    if (right_ != null) {
        right_->add_root(this);
    }
    if (third_ != null) {
        third_->add_root(this);
    }
}

ExpressionImpl::~ExpressionImpl() {
    // Check if eTbl is still valid (not destroyed during static destruction)
    if (!eTbl_destroyed) {
        eTbl().erase(this);
    }
    if (left() != null) {
        left()->remove_root(this);
    }
    if (right() != null) {
        right()->remove_root(this);
    }
    if (third() != null) {
        third()->remove_root(this);
    }
    // CUSTOM_FUNCTION ノードの場合、custom_args_ の親子関係も解除
    if (op_ == CUSTOM_FUNCTION) {
        for (const Expression& arg : custom_args_) {
            if (arg != null) {
                arg->remove_root(this);
            }
        }
    }
}

double ExpressionImpl::value() {
    if (!is_set_value()) {
        // Check CUSTOM_FUNCTION operation first
        if (op_ == CUSTOM_FUNCTION) {
            std::vector<double> args_values;
            args_values.reserve(custom_args_.size());
            for (const Expression& e : custom_args_) {
                args_values.push_back(e->value());
            }

            double v = custom_func_->value(args_values);

            value_ = v;
            is_set_value_ = true;
            return v;
        }

        // Check CONST and PARAM operations
        if (op_ == CONST) {
            // CONST nodes should always have is_set_value_ == true
            // This branch should not be reached, but handle it for safety
            return value_;
        }
        if (op_ == PARAM) {
            // PARAM nodes (Variables) must have value set before calling value()
            // This branch should not be reached if Variable::operator= was called
            throw Nh::RuntimeException("Expression: variable value not set");
        }

        // Check 3-argument operations first
        if (left() != null && right() != null && third() != null) {
            double h = left()->value();
            double k = right()->value();
            double rho = third()->value();

            if (op_ == PHI2) {
                // Φ₂(h, k; ρ) using numerical integration (Simpson's rule)
                value_ = compute_Phi2(h, k, rho);
            } else {
                throw Nh::RuntimeException("Expression: invalid 3-argument operation");
            }

        } else if (left() != null && right() != null) {
            // Binary operations
            double l = left()->value();
            double r = right()->value();

            if (op_ == PLUS) {
                value_ = l + r;
            } else if (op_ == MINUS) {
                value_ = l - r;
            } else if (op_ == MUL) {
                value_ = l * r;
            } else if (op_ == DIV) {
                if (r == 0.0) {
                    throw Nh::RuntimeException("Expression: division by zero");
                }
                value_ = l / r;
            } else if (op_ == POWER) {
                value_ = std::pow(l, r);
            } else {
                throw Nh::RuntimeException("Expression: invalid operation");
            }

        } else if (left() != null) {
            double l = left()->value();

            if (op_ == EXP) {
                value_ = std::exp(l);
            } else if (op_ == LOG) {
                if (l < 0.0) {
                    throw Nh::RuntimeException("Expression: logarithm of negative number");
                }
                value_ = std::log(l);
            } else if (op_ == ERF) {
                value_ = std::erf(l);
            } else if (op_ == SQRT) {
                if (l < 0.0) {
                    throw Nh::RuntimeException("Expression: square root of negative number");
                }
                value_ = std::sqrt(l);
            } else {
                throw Nh::RuntimeException("Expression: invalid unary operation");
            }

        } else {
            throw Nh::RuntimeException("Expression: invalid operation");
        }
        is_set_value_ = true;
    }
    return (value_);
}

void ExpressionImpl::set_value(double value) {
    unset_root_value();
    _set_value(value);
}

void ExpressionImpl::_set_value(double value) {
    value_ = value;
    is_set_value_ = true;
}

void ExpressionImpl::unset_value() {
    if (!is_set_value()) {
        return;
    }

    unset_root_value();
    value_ = 0.0;
    is_set_value_ = false;
}

void ExpressionImpl::unset_root_value() {
    for_each(roots_.begin(), roots_.end(), std::mem_fn(&ExpressionImpl::unset_value));
}

void ExpressionImpl::zero_grad() {
    gradient_ = 0.0;
    is_gradient_set_ = false;
}

void ExpressionImpl::zero_all_grad() {
    for_each(eTbl().begin(), eTbl().end(), std::mem_fn(&ExpressionImpl::zero_grad));
}

size_t ExpressionImpl::node_count() {
    return eTbl().size();
}

// Helper: propagate gradient from this node to its children (called once per node)
void ExpressionImpl::propagate_gradient() {
    // For leaf nodes, nothing to propagate
    if (op_ == CONST || op_ == PARAM) {
        return;
    }
    
    double upstream = gradient_;
    
    // Check CUSTOM_FUNCTION operation first
    if (op_ == CUSTOM_FUNCTION) {
        std::vector<double> args_values;
        args_values.reserve(custom_args_.size());
        for (const Expression& e : custom_args_) {
            args_values.push_back(e->value());
        }

        auto [val, grad_vec] = custom_func_->eval_with_gradient(args_values);

        const size_t n = custom_args_.size();
        if (grad_vec.size() != n) {
            throw Nh::RuntimeException(
                "CustomFunction::eval_with_gradient: gradient size mismatch");
        }

        for (size_t i = 0; i < n; ++i) {
            double contrib = upstream * grad_vec[i];
            custom_args_[i]->gradient_ += contrib;
        }
        return;
    }
    
    // Check 3-argument operations first
    if (left() != null && right() != null && third() != null) {
        double h = left()->value();
        double k = right()->value();
        double rho = third()->value();

        if (op_ == PHI2) {
            double one_minus_rho2 = 1.0 - (rho * rho);
            double sigma = std::sqrt(std::max(1e-12, one_minus_rho2));

            double grad_h = normal_pdf(h) * normal_cdf((k - rho * h) / sigma);
            double grad_k = normal_pdf(k) * normal_cdf((h - rho * k) / sigma);
            double grad_rho = compute_phi2(h, k, rho);

            left()->gradient_ += upstream * grad_h;
            right()->gradient_ += upstream * grad_k;
            third()->gradient_ += upstream * grad_rho;
        }
        return;
    }

    // Binary operations
    if (left() != null && right() != null) {
        double l = left()->value();
        double r = right()->value();

        if (op_ == PLUS) {
            left()->gradient_ += upstream;
            right()->gradient_ += upstream;
        } else if (op_ == MINUS) {
            left()->gradient_ += upstream;
            right()->gradient_ += -upstream;
        } else if (op_ == MUL) {
            left()->gradient_ += upstream * r;
            right()->gradient_ += upstream * l;
        } else if (op_ == DIV) {
            left()->gradient_ += upstream / r;
            right()->gradient_ += -upstream * l / (r * r);
        } else if (op_ == POWER) {
            double f_val = value();
            left()->gradient_ += upstream * r * std::pow(l, r - 1);
            if (l > 0) {
                right()->gradient_ += upstream * f_val * std::log(l);
            }
        }
    } else if (left() != null) {
        // Unary operations
        double l = left()->value();

        if (op_ == EXP) {
            left()->gradient_ += upstream * value();
        } else if (op_ == LOG) {
            left()->gradient_ += upstream / l;
        } else if (op_ == ERF) {
            static constexpr double TWO_OVER_SQRT_PI = 1.1283791670955126;
            left()->gradient_ += upstream * TWO_OVER_SQRT_PI * std::exp(-l * l);
        } else if (op_ == SQRT) {
            double sqrt_l = value();
            left()->gradient_ += upstream / (2.0 * sqrt_l);
        }
    }
}

// Topological sort helper: DFS to build reverse topological order
static void topo_sort_dfs(ExpressionImpl* node, 
                          std::set<ExpressionImpl*>& visited,
                          std::vector<ExpressionImpl*>& order) {
    if (visited.count(node) > 0) {
        return;
    }
    visited.insert(node);
    
    // CUSTOM_FUNCTION ノードの場合は custom_args_ を子として辿る
    if (node->op() == ExpressionImpl::CUSTOM_FUNCTION) {
        for (const Expression& arg : node->custom_args()) {
            if (arg.get() != nullptr) {
                topo_sort_dfs(arg.get(), visited, order);
            }
        }
    } else {
        // 既存ロジック: left/right/third
        if (node->left().get() != nullptr) {
            topo_sort_dfs(node->left().get(), visited, order);
        }
        if (node->right().get() != nullptr) {
            topo_sort_dfs(node->right().get(), visited, order);
        }
        if (node->third().get() != nullptr) {
            topo_sort_dfs(node->third().get(), visited, order);
        }
    }
    
    // Add this node after children (reverse topological order)
    order.push_back(node);
}

void ExpressionImpl::backward(double upstream) {
    // Build topological order (children before parents)
    std::set<ExpressionImpl*> visited;
    std::vector<ExpressionImpl*> topo_order;
    topo_sort_dfs(this, visited, topo_order);
    
    // Set initial gradient at the root
    gradient_ += upstream;
    is_gradient_set_ = true;
    
    // Process in reverse order (parents before children)
    // This ensures all upstream gradients are accumulated before propagation
    for (auto it = topo_order.rbegin(); it != topo_order.rend(); ++it) {
        (*it)->propagate_gradient();
        (*it)->is_gradient_set_ = true;
    }
}

void ExpressionImpl::add_root(ExpressionImpl* root) {
    roots_.insert(root);
}

void ExpressionImpl::remove_root(ExpressionImpl* root) {
    roots_.erase(root);
}

void ExpressionImpl::print() {
    std::cout << std::setw(5) << id_;

    if (is_set_value()) {
        std::cout << std::fixed << std::setprecision(4) << std::setw(10) << value();
    } else {
        std::cout << std::setw(10) << "--";
    }

    if (op_ == CONST) {
        std::cout << std::setw(10) << "CONST";
    } else if (op_ == PARAM) {
        std::cout << std::setw(10) << "PARAM";
    } else if (op_ == PLUS) {
        std::cout << std::setw(10) << "+";
    } else if (op_ == MINUS) {
        std::cout << std::setw(10) << "-";
    } else if (op_ == MUL) {
        std::cout << std::setw(10) << "x";
    } else if (op_ == DIV) {
        std::cout << std::setw(10) << "/";
    } else if (op_ == EXP) {
        std::cout << std::setw(10) << "exp";
    } else if (op_ == LOG) {
        std::cout << std::setw(10) << "log";
    } else if (op_ == ERF) {
        std::cout << std::setw(10) << "erf";
    } else if (op_ == SQRT) {
        std::cout << std::setw(10) << "sqrt";
    } else if (op_ == POWER) {
        std::cout << std::setw(10) << "^";
    } else if (op_ == PHI2) {
        std::cout << std::setw(10) << "PHI2";
    } else if (op_ == CUSTOM_FUNCTION) {
        std::ostringstream oss;
        oss << "CUSTOM(" << custom_func_->name()
            << ", n=" << custom_args_.size() << ")";
        std::cout << std::setw(10) << oss.str();
    }

    if (left() != null) {
        if (left()->is_set_value()) {
            std::cout << std::fixed << std::setprecision(4) << std::setw(10) << left()->value();
        } else {
            std::cout << std::setw(10) << "--";
        }
    } else {
        std::cout << std::setw(10) << "--";
    }

    if (right() != null) {
        if (right()->is_set_value()) {
            std::cout << std::fixed << std::setprecision(4) << std::setw(10) << right()->value();
        } else {
            std::cout << std::setw(10) << "--";
        }
    } else {
        std::cout << std::setw(10) << "--";
    }

    if (left() != null) {
        std::cout << std::setw(10) << left()->id();
    } else {
        std::cout << std::setw(10) << "--";
    }

    if (right() != null) {
        std::cout << std::setw(10) << right()->id();
    } else {
        std::cout << std::setw(10) << "--";
    }

    std::cout << std::endl;
}

void ExpressionImpl::print_all() {
    for_each(eTbl().begin(), eTbl().end(), std::mem_fn(&ExpressionImpl::print));
}

void print_all() {
    ExpressionImpl::print_all();
}

//////////////////////////

Const::Const(double value)
    : Expression(std::make_shared<ConstImpl>(value)) {}

//////////////////////////

double VariableImpl::value() {
    if (!is_set_value()) {
        throw Nh::RuntimeException("Expression: variable value not set");
    }
    return (value_);
}

Variable::Variable()
    : Expression(std::make_shared<VariableImpl>()) {}

Variable& Variable::operator=(double value) {
    (*this)->set_value(value);
    return *this;
}

//////////////////////////

Expression operator+(const Expression& a, const Expression& b) {
    if (a == zero) {
        return b;
    }
    if (b == zero) {
        return a;
    }
    return Expression(std::make_shared<ExpressionImpl>(ExpressionImpl::PLUS, a, b));
}

Expression operator+(double a, const Expression& b) {
    return (Const(a) + b);
}

Expression operator+(const Expression& a, double b) {
    return (b + a);
}

Expression operator+(const Expression& a) {
    if (a == zero) {
        return zero;
    }
    return (a);
}

//////////////////////////

Expression operator-(const Expression& a, const Expression& b) {
    if (a == zero) {
        return (-b);
    }
    if (b == zero) {
        return a;
    }
    return Expression(std::make_shared<ExpressionImpl>(ExpressionImpl::MINUS, a, b));
}

Expression operator-(double a, const Expression& b) {
    return (Const(a) - b);
}

Expression operator-(const Expression& a, double b) {
    return ((-b) + a);
}

Expression operator-(const Expression& a) {
    if (a == zero) {
        return zero;
    }
    if (a == minus_one) {
        return one;
    }
    return Expression(std::make_shared<ExpressionImpl>(ExpressionImpl::MUL, minus_one, a));
}

//////////////////////////

Expression operator*(const Expression& a, const Expression& b) {
    if (a == zero || b == zero) {
        return zero;
    }
    if (a == one) {
        return b;
    }
    if (b == one) {
        return a;
    }
    return Expression(std::make_shared<ExpressionImpl>(ExpressionImpl::MUL, a, b));
}

Expression operator*(double a, const Expression& b) {
    return (Const(a) * b);
}

Expression operator*(const Expression& a, double b) {
    return (b * a);
}

//////////////////////////

Expression operator/(const Expression& a, const Expression& b) {
    if (b == zero) {
        throw Nh::RuntimeException("Expression: division by zero");
    }
    if (a == zero) {
        return zero;
    }
    if (b == one) {
        return a;
    }
    if (b == minus_one) {
        return (-a);
    }
    if (a == b) {
        return one;
    }
    return Expression(std::make_shared<ExpressionImpl>(ExpressionImpl::DIV, a, b));
}

Expression operator/(const Expression& a, double b) {
    return (a / Const(b));
}

Expression operator/(double a, const Expression& b) {
    return (Const(a) / b);
}

//////////////////////////

Expression operator^(const Expression& a, const Expression& b) {
    if (b == zero) {
        if (a == zero) {
            throw Nh::RuntimeException("Expression: zero to the power of zero");
        }
        return one;
    }
    if (b == one) {
        return a;
    }
    if (a == zero) {
        return zero;
    }
    return Expression(std::make_shared<ExpressionImpl>(ExpressionImpl::POWER, a, b));
}

Expression operator^(const Expression& a, double b) {
    return (a ^ Const(b));
}

Expression operator^(double a, const Expression& b) {
    return (Const(a) ^ b);
}

//////////////////////////

Expression exp(const Expression& a) {
    return Expression(std::make_shared<ExpressionImpl>(ExpressionImpl::EXP, a, null));
}

Expression log(const Expression& a) {
    return Expression(std::make_shared<ExpressionImpl>(ExpressionImpl::LOG, a, null));
}

Expression erf(const Expression& a) {
    return Expression(std::make_shared<ExpressionImpl>(ExpressionImpl::ERF, a, null));
}

Expression sqrt(const Expression& a) {
    return Expression(std::make_shared<ExpressionImpl>(ExpressionImpl::SQRT, a, null));
}

//////////////////////////

// assignment to (double)
double& operator<<(double& a, const Expression& b) {
    a = b->value();
    return a;
}

//////////////////////////
// Automatic differentiation utilities

void zero_all_grad() {
    ExpressionImpl::zero_all_grad();
}

//////////////////////////
// Bivariate normal distribution expressions (Phase 5 of #167)

// Cache for expected_prod_pos_expr (pointer-based)
// Key: tuple of ExpressionImpl pointers (mu0, sigma0, mu1, sigma1, rho)
using ExpectedProdPosCacheKey = std::tuple<ExpressionImpl*, ExpressionImpl*, ExpressionImpl*, ExpressionImpl*, ExpressionImpl*>;

// Hash function for tuple of pointers
struct ExpectedProdPosCacheKeyHash {
    std::size_t operator()(const ExpectedProdPosCacheKey& key) const {
        std::size_t h1 = std::hash<ExpressionImpl*>{}(std::get<0>(key));
        std::size_t h2 = std::hash<ExpressionImpl*>{}(std::get<1>(key));
        std::size_t h3 = std::hash<ExpressionImpl*>{}(std::get<2>(key));
        std::size_t h4 = std::hash<ExpressionImpl*>{}(std::get<3>(key));
        std::size_t h5 = std::hash<ExpressionImpl*>{}(std::get<4>(key));
        return h1 ^ (h2 << 1) ^ (h3 << 2) ^ (h4 << 3) ^ (h5 << 4);
    }
};

static std::unordered_map<ExpectedProdPosCacheKey, Expression, ExpectedProdPosCacheKeyHash>
    expected_prod_pos_expr_cache;

// Cache statistics
static size_t expected_prod_pos_cache_hits = 0;
static size_t expected_prod_pos_cache_misses = 0;


// Cache for phi_expr (pointer-based)
static std::unordered_map<ExpressionImpl*, Expression> phi_expr_cache;
static size_t phi_expr_cache_hits = 0;
static size_t phi_expr_cache_misses = 0;

Expression phi_expr(const Expression& x) {
    // φ(x) = exp(-x²/2) / √(2π)
    
    // Check cache first
    auto it = phi_expr_cache.find(x.get());
    if (it != phi_expr_cache.end()) {
        phi_expr_cache_hits++;
        return it->second;
    }
    
    phi_expr_cache_misses++;
    
    static constexpr double INV_SQRT_2PI = 0.3989422804014327;  // 1/√(2π)
    // Create a new Const(2.0) each time to avoid issues with global 'two' being cleared
    // in custom function internal expression trees
    Expression result = INV_SQRT_2PI * exp(-(x * x) / Const(2.0));
    
    // Cache the result
    phi_expr_cache[x.get()] = result;
    
    return result;
}

// Cache for Phi_expr (pointer-based)
static std::unordered_map<ExpressionImpl*, Expression> Phi_expr_cache;
static size_t Phi_expr_cache_hits = 0;
static size_t Phi_expr_cache_misses = 0;

Expression Phi_expr(const Expression& x) {
    // Φ(x) = 0.5 × (1 + erf(x/√2))
    
    // Check cache first
    auto it = Phi_expr_cache.find(x.get());
    if (it != Phi_expr_cache.end()) {
        Phi_expr_cache_hits++;
        return it->second;
    }
    
    Phi_expr_cache_misses++;
    
    static constexpr double INV_SQRT_2 = 0.7071067811865476;  // 1/√2
    Expression result = half * (one + erf(x * INV_SQRT_2));
    
    // Cache the result
    Phi_expr_cache[x.get()] = result;
    
    return result;
}

Expression expected_prod_pos_expr(const Expression& mu0, const Expression& sigma0,
                                  const Expression& mu1, const Expression& sigma1,
                                  const Expression& rho) {
    // E[D0⁺ D1⁺] where D0, D1 are bivariate normal
    //
    // Formula:
    // E[D0⁺ D1⁺] = μ0 μ1 Φ₂(a0, a1; ρ)
    //            + μ0 σ1 φ(a1) Φ((a0 - ρa1)/√(1-ρ²))
    //            + μ1 σ0 φ(a0) Φ((a1 - ρa0)/√(1-ρ²))
    //            + σ0 σ1 [ρ Φ₂(a0, a1; ρ) + (1-ρ²) φ₂(a0, a1; ρ)]
    //
    // where a0 = μ0/σ0, a1 = μ1/σ1

    // Check cache first (pointer-based)
    auto key = std::make_tuple(mu0.get(), sigma0.get(), mu1.get(), sigma1.get(), rho.get());
    auto it = expected_prod_pos_expr_cache.find(key);
    if (it != expected_prod_pos_expr_cache.end()) {
        expected_prod_pos_cache_hits++;
        return it->second;
    }
    
    expected_prod_pos_cache_misses++;

    Expression a0 = mu0 / sigma0;
    Expression a1 = mu1 / sigma1;
    Expression one_minus_rho2 = one - rho * rho;
    Expression sqrt_one_minus_rho2 = sqrt(one_minus_rho2);

    // Φ₂(a0, a1; ρ) - create PHI2 node directly to avoid dependency on Phi2_expr
    Expression Phi2_a0_a1 = Expression(std::make_shared<ExpressionImpl>(ExpressionImpl::PHI2, a0, a1, rho));

    // φ(a0) and φ(a1)
    Expression phi_a0 = phi_expr(a0);
    Expression phi_a1 = phi_expr(a1);

    // Φ((a0 - ρa1)/√(1-ρ²)) and Φ((a1 - ρa0)/√(1-ρ²))
    Expression Phi_cond_0 = Phi_expr((a0 - rho * a1) / sqrt_one_minus_rho2);
    Expression Phi_cond_1 = Phi_expr((a1 - rho * a0) / sqrt_one_minus_rho2);

    // φ₂(a0, a1; ρ) = 1/(2π√(1-ρ²)) × exp(-(a0² - 2ρa0a1 + a1²)/(2(1-ρ²)))
    // Direct implementation to avoid dependency on phi2_expr
    Expression coeff_phi2 = one / (Const(2.0 * M_PI) * sqrt_one_minus_rho2);
    Expression Q_phi2 = (a0 * a0 - two * rho * a0 * a1 + a1 * a1) / one_minus_rho2;
    Expression phi2_a0_a1 = coeff_phi2 * exp(-Q_phi2 / two);

    // Build the formula
    Expression term1 = mu0 * mu1 * Phi2_a0_a1;
    Expression term2 = mu0 * sigma1 * phi_a1 * Phi_cond_0;
    Expression term3 = mu1 * sigma0 * phi_a0 * Phi_cond_1;
    Expression term4 = sigma0 * sigma1 * (rho * Phi2_a0_a1 + one_minus_rho2 * phi2_a0_a1);
    Expression result = term1 + term2 + term3 + term4;
    
    // Cache the result
    expected_prod_pos_expr_cache[key] = result;
    
    return result;
}

// Expose cache statistics for debugging
size_t get_expected_prod_pos_cache_hits() {
    return expected_prod_pos_cache_hits;
}

size_t get_expected_prod_pos_cache_misses() {
    return expected_prod_pos_cache_misses;
}

size_t get_phi_expr_cache_hits() {
    return phi_expr_cache_hits;
}

size_t get_phi_expr_cache_misses() {
    return phi_expr_cache_misses;
}

size_t get_Phi_expr_cache_hits() {
    return Phi_expr_cache_hits;
}

size_t get_Phi_expr_cache_misses() {
    return Phi_expr_cache_misses;
}

// E[D0⁺ D1⁺] for ρ = 1 (perfectly correlated)
// When ρ = 1: D0 = μ0 + σ0·Z, D1 = μ1 + σ1·Z (same Z)
// Both positive when Z > c where c = -min(a0, a1), a0 = μ0/σ0, a1 = μ1/σ1
// E[D0⁺ D1⁺] = σ0·σ1 · [(a0·a1 + 1)·Φ(-c) + (a0 + a1 + c)·φ(c)]
Expression expected_prod_pos_rho1_expr(const Expression& mu0, const Expression& sigma0,
                                       const Expression& mu1, const Expression& sigma1) {
    Expression a0 = mu0 / sigma0;
    Expression a1 = mu1 / sigma1;

    // c = -min(a0, a1)
    // For differentiability, we compute both cases and use the appropriate one
    // When a0 < a1: c = -a0, when a0 >= a1: c = -a1
    double a0_val = a0->value();
    double a1_val = a1->value();
    Expression c = (a0_val < a1_val) ? (minus_one * a0) : (minus_one * a1);

    // E[D0⁺ D1⁺] = σ0·σ1 · [(a0·a1 + 1)·Φ(-c) + (a0 + a1 + c)·φ(c)]
    Expression Phi_neg_c = Phi_expr(minus_one * c);
    Expression phi_c = phi_expr(c);

    Expression result = sigma0 * sigma1 *
                        ((a0 * a1 + one) * Phi_neg_c + (a0 + a1 + c) * phi_c);

    return result;
}

// E[D0⁺ D1⁺] for ρ = -1 (perfectly negatively correlated)
// When ρ = -1: D0 = μ0 + σ0·Z, D1 = μ1 - σ1·Z (opposite signs)
// Both positive when -a0 < Z < a1 (if a0 + a1 > 0)
// E[D0⁺ D1⁺] = σ0·σ1 · [(a0·a1 - 1)·(Φ(a0) + Φ(a1) - 1) + a1·φ(a0) + a0·φ(a1)]
// Returns 0 if a0 + a1 <= 0 (interval is empty)
Expression expected_prod_pos_rho_neg1_expr(const Expression& mu0, const Expression& sigma0,
                                           const Expression& mu1, const Expression& sigma1) {
    Expression a0 = mu0 / sigma0;
    Expression a1 = mu1 / sigma1;

    // Check if interval is non-empty: a0 + a1 > 0
    double a0_val = a0->value();
    double a1_val = a1->value();

    if (a0_val + a1_val <= 0.0) {
        // Interval is empty, E[D0⁺ D1⁺] = 0
        return Const(0.0);
    }

    // E[D0⁺ D1⁺] = σ0·σ1 · [(a0·a1 - 1)·(Φ(a0) + Φ(a1) - 1) + a1·φ(a0) + a0·φ(a1)]
    Expression Phi_a0 = Phi_expr(a0);
    Expression Phi_a1 = Phi_expr(a1);
    Expression phi_a0 = phi_expr(a0);
    Expression phi_a1 = phi_expr(a1);

    Expression result = sigma0 * sigma1 *
                        ((a0 * a1 - one) * (Phi_a0 + Phi_a1 - one) +
                         a1 * phi_a0 + a0 * phi_a1);

    return result;
}


//////////////////////////

//////////////////////////
// Custom Function Implementation

ExpressionImpl::ExpressionImpl(const CustomFunctionHandle& func,
                               const std::vector<Expression>& args)
    : id_(current_id_++)
    , is_set_value_(false)
    , value_(0.0)
    , gradient_(0.0)
    , is_gradient_set_(false)
    , op_(CUSTOM_FUNCTION)
    , left_(null)
    , right_(null)
    , third_(null)
    , custom_func_(func)
    , custom_args_(args) {
    eTbl().insert(this);
    // Add this node as root to all argument expressions
    for (const Expression& arg : custom_args_) {
        if (arg) {
            arg->add_root(this);
        }
    }
}

CustomFunctionImpl::CustomFunctionImpl(size_t input_dim,
                                       const Builder& builder,
                                       const std::string& name)
    : input_dim_(input_dim) {
    // Determine name
    if (name.empty()) {
        static std::atomic<size_t> counter{0};
        size_t id = counter++;
        name_ = "custom_f_" + std::to_string(id);
    } else {
        name_ = name;
    }

    // 1. Create local input variables
    local_vars_.resize(input_dim_);
    for (size_t i = 0; i < input_dim_; ++i) {
        local_vars_[i] = Variable();
    }

    // 2. Call builder to construct expression tree
    output_ = builder(local_vars_);

    // 3. Build list of all nodes in internal expression tree
    build_nodes_list();
}

void CustomFunctionImpl::build_nodes_list() {
    nodes_.clear();
    std::unordered_set<ExpressionImpl*> visited;
    if (output_) {
        collect_nodes_dfs(output_.get(), visited);
    }
}

void CustomFunctionImpl::collect_nodes_dfs(ExpressionImpl* node,
                                           std::unordered_set<ExpressionImpl*>& visited) {
    if (node == nullptr || visited.find(node) != visited.end()) {
        return;
    }

    visited.insert(node);
    nodes_.push_back(node);

    // CUSTOM_FUNCTION ノードなら custom_args_ を辿る
    if (node->op() == ExpressionImpl::CUSTOM_FUNCTION) {
        for (const Expression& arg : node->custom_args()) {
            if (arg) {
                collect_nodes_dfs(arg.get(), visited);
            }
        }
    } else {
        // 通常どおり left/right/third を辿る
        if (node->left()) {
            collect_nodes_dfs(node->left().get(), visited);
        }
        if (node->right()) {
            collect_nodes_dfs(node->right().get(), visited);
        }
        if (node->third()) {
            collect_nodes_dfs(node->third().get(), visited);
        }
    }
}

void CustomFunctionImpl::set_inputs_and_clear(const std::vector<double>& x) {
    if (x.size() != input_dim_) {
        throw Nh::RuntimeException(
            "CustomFunctionImpl::set_inputs_and_clear: size mismatch");
    }

    // 1. Clear value cache and gradients in internal expression tree
    for (auto* node : nodes_) {
        // 1.1. 勾配は全ノードでクリア
        node->zero_grad();

        // 1.2. ローカル入力変数は value を直接上書きするので、
        //      キャッシュクリアは上流側（root）からやればいい。
        bool is_local_var = false;
        for (size_t i = 0; i < input_dim_; ++i) {
            if (node == local_vars_[i].get()) {
                is_local_var = true;
                break;
            }
        }
        if (is_local_var) {
            continue;
        }

        // 1.3. CONST ノードはグローバル共有なので value_ を触らない
        if (node->op() == ExpressionImpl::CONST) {
            continue;
        }

        // 1.4. それ以外のノードのキャッシュは無条件でクリア
        //      unset_value() 側ですでに if (!is_set_value()) return; ガードが入っているので、
        //      呼ぶ前に is_set_value() を見る必要はない
        node->unset_value();
    }

    // 2. 最後にローカル入力変数に値を入れる
    for (size_t i = 0; i < input_dim_; ++i) {
        local_vars_[i] = x[i];
    }
}

bool CustomFunctionImpl::args_equal(const std::vector<double>& a,
                                    const std::vector<double>& b) {
    if (a.size() != b.size()) {
        return false;
    }
    for (size_t i = 0; i < a.size(); ++i) {
        if (a[i] != b[i]) {
            return false;
        }
    }
    return true;
}

double CustomFunctionImpl::value(const std::vector<double>& x) {
    set_inputs_and_clear(x);
    double v = output_->value();
    // Cache the value and arguments for potential reuse in eval_with_gradient
    last_args_ = x;
    last_value_ = v;
    has_cached_value_ = true;
    return v;
}

std::vector<double> CustomFunctionImpl::gradient(const std::vector<double>& x) {
    set_inputs_and_clear(x);
    output_->value();
    output_->backward(1.0);

    std::vector<double> grad(input_dim_);
    for (size_t i = 0; i < input_dim_; ++i) {
        grad[i] = local_vars_[i]->gradient();
    }
    return grad;
}

std::pair<double, std::vector<double>>
CustomFunctionImpl::value_and_gradient(const std::vector<double>& x) {
    set_inputs_and_clear(x);
    double v = output_->value();
    output_->backward(1.0);

    std::vector<double> grad(input_dim_);
    for (size_t i = 0; i < input_dim_; ++i) {
        grad[i] = local_vars_[i]->gradient();
    }
    return {v, std::move(grad)};
}

std::pair<double, std::vector<double>>
CustomFunctionImpl::eval_with_gradient(const std::vector<double>& args_values) {
    // Optimization: if value was already computed with the same arguments,
    // reuse it and only compute gradient
    double v = 0.0;
    if (has_cached_value_ && args_equal(args_values, last_args_)) {
        // Reuse cached value, only compute gradient
        v = last_value_;
        // Still need to set inputs for gradient computation
        set_inputs_and_clear(args_values);
        // Value is already computed, skip value() call
        output_->backward(1.0);
    } else {
        // Compute both value and gradient
        return value_and_gradient(args_values);
    }

    std::vector<double> grad(input_dim_);
    for (size_t i = 0; i < input_dim_; ++i) {
        grad[i] = local_vars_[i]->gradient();
    }
    return {v, std::move(grad)};
}

Expression CustomFunction::operator()(const std::vector<Expression>& args) const {
    ensure_valid();
    if (args.size() != impl_->input_dim()) {
        throw Nh::RuntimeException(
            "CustomFunction::operator(): argument count mismatch");
    }
    return make_custom_call(impl_, args);
}

Expression CustomFunction::operator()(std::initializer_list<Expression> args) const {
    return (*this)(std::vector<Expression>(args));
}

Expression make_custom_call(const CustomFunctionHandle& func,
                            const std::vector<Expression>& args) {
    // Create a new ExpressionImpl node with CUSTOM_FUNCTION operation
    auto impl = std::make_shared<ExpressionImpl>(func, args);
    return Expression(impl);
}
