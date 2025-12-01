// -*- c++ -*-
// Author: Jiro Iwai

#include "expression.hpp"
#include "util_numerical.hpp"

#include <algorithm>
#include <cmath>
#include <functional>
#include <iomanip>
#include <iostream>
#include <memory>
#include <sstream>
#include <tuple>
#include <unordered_map>
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
}

double ExpressionImpl::value() {
    if (!is_set_value()) {
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
    
    // Visit children first
    if (node->left().get() != nullptr) {
        topo_sort_dfs(node->left().get(), visited, order);
    }
    if (node->right().get() != nullptr) {
        topo_sort_dfs(node->right().get(), visited, order);
    }
    if (node->third().get() != nullptr) {
        topo_sort_dfs(node->third().get(), visited, order);
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

// Cache for phi2_expr (pointer-based)
using Phi2PDFCacheKey = std::tuple<ExpressionImpl*, ExpressionImpl*, ExpressionImpl*>;

struct Phi2PDFCacheKeyHash {
    std::size_t operator()(const Phi2PDFCacheKey& key) const {
        std::size_t h1 = std::hash<ExpressionImpl*>{}(std::get<0>(key));
        std::size_t h2 = std::hash<ExpressionImpl*>{}(std::get<1>(key));
        std::size_t h3 = std::hash<ExpressionImpl*>{}(std::get<2>(key));
        return h1 ^ (h2 << 1) ^ (h3 << 2);
    }
};

static std::unordered_map<Phi2PDFCacheKey, Expression, Phi2PDFCacheKeyHash> phi2_pdf_expr_cache;

Expression phi2_expr(const Expression& x, const Expression& y, const Expression& rho,
                     const Expression& one_minus_rho2, const Expression& sqrt_one_minus_rho2) {
    // φ₂(x, y; ρ) = 1/(2π√(1-ρ²)) × exp(-(x² - 2ρxy + y²)/(2(1-ρ²)))
    
    // Check cache first
    auto key = std::make_tuple(x.get(), y.get(), rho.get());
    auto it = phi2_pdf_expr_cache.find(key);
    if (it != phi2_pdf_expr_cache.end()) {
        return it->second;
    }
    
    // Use provided one_minus_rho2 and sqrt_one_minus_rho2 if available, otherwise compute them
    Expression one_minus_rho2_local;
    Expression sqrt_one_minus_rho2_local;
    
    if (one_minus_rho2 && sqrt_one_minus_rho2) {
        // Use provided values (optimization: avoid redundant computation)
        one_minus_rho2_local = one_minus_rho2;
        sqrt_one_minus_rho2_local = sqrt_one_minus_rho2;
    } else {
        // Compute internally (backward compatibility)
        one_minus_rho2_local = one - rho * rho;
        sqrt_one_minus_rho2_local = sqrt(one_minus_rho2_local);
    }
    
    Expression coeff = one / (Const(2.0 * M_PI) * sqrt_one_minus_rho2_local);
    Expression Q = (x * x - two * rho * x * y + y * y) / one_minus_rho2_local;
    Expression result = coeff * exp(-Q / two);
    
    // Cache the result
    phi2_pdf_expr_cache[key] = result;
    
    return result;
}

// Cache for Phi2_expr (pointer-based)
using Phi2CacheKey = std::tuple<ExpressionImpl*, ExpressionImpl*, ExpressionImpl*>;

struct Phi2CacheKeyHash {
    std::size_t operator()(const Phi2CacheKey& key) const {
        std::size_t h1 = std::hash<ExpressionImpl*>{}(std::get<0>(key));
        std::size_t h2 = std::hash<ExpressionImpl*>{}(std::get<1>(key));
        std::size_t h3 = std::hash<ExpressionImpl*>{}(std::get<2>(key));
        return h1 ^ (h2 << 1) ^ (h3 << 2);
    }
};

static std::unordered_map<Phi2CacheKey, Expression, Phi2CacheKeyHash> phi2_expr_cache;

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
    Expression result = INV_SQRT_2PI * exp(-(x * x) / two);
    
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

Expression Phi2_expr(const Expression& h, const Expression& k, const Expression& rho) {
    // Φ₂(h, k; ρ) using numerical integration for value, analytical gradients
    
    // Check cache first
    auto key = std::make_tuple(h.get(), k.get(), rho.get());
    auto it = phi2_expr_cache.find(key);
    if (it != phi2_expr_cache.end()) {
        return it->second;
    }
    
    Expression result = Expression(std::make_shared<ExpressionImpl>(ExpressionImpl::PHI2, h, k, rho));
    
    // Cache the result
    phi2_expr_cache[key] = result;
    
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

    // Φ₂(a0, a1; ρ)
    Expression Phi2_a0_a1 = Phi2_expr(a0, a1, rho);

    // φ(a0) and φ(a1)
    Expression phi_a0 = phi_expr(a0);
    Expression phi_a1 = phi_expr(a1);

    // Φ((a0 - ρa1)/√(1-ρ²)) and Φ((a1 - ρa0)/√(1-ρ²))
    Expression Phi_cond_0 = Phi_expr((a0 - rho * a1) / sqrt_one_minus_rho2);
    Expression Phi_cond_1 = Phi_expr((a1 - rho * a0) / sqrt_one_minus_rho2);

    // φ₂(a0, a1; ρ) - use precomputed one_minus_rho2 and sqrt_one_minus_rho2
    Expression phi2_a0_a1 = phi2_expr(a0, a1, rho, one_minus_rho2, sqrt_one_minus_rho2);

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

Expression cov_max0_max0_expr(const Expression& mu0, const Expression& sigma0,
                              const Expression& mu1, const Expression& sigma1,
                              const Expression& rho) {
    // Cov(D0⁺, D1⁺) = E[D0⁺ D1⁺] - E[D0⁺] × E[D1⁺]
    //
    // E[D0⁺] = expected_positive_part(μ0, σ0) = σ0 × MeanMax(-μ0/σ0) + μ0
    // But using max0_mean_expr for consistency
    Expression E_D0_pos = max0_mean_expr(mu0, sigma0);
    Expression E_D1_pos = max0_mean_expr(mu1, sigma1);

    Expression E_prod = expected_prod_pos_expr(mu0, sigma0, mu1, sigma1, rho);

    return E_prod - E_D0_pos * E_D1_pos;
}

//////////////////////////
