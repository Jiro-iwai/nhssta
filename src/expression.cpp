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
                value_ = RandomVariable::bivariate_normal_cdf(h, k, rho);
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
            // Unary operations
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

            double grad_h = RandomVariable::normal_pdf(h) * RandomVariable::normal_cdf((k - rho * h) / sigma);
            double grad_k = RandomVariable::normal_pdf(k) * RandomVariable::normal_cdf((h - rho * k) / sigma);
            double grad_rho = RandomVariable::bivariate_normal_pdf(h, k, rho);

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
        const auto& func = custom_func_;
        const char* kind_str =
            (func->kind() == CustomFunctionImpl::ImplKind::Graph)
                ? "G"
                : "N";  // Graph / Native

        std::ostringstream oss;
        oss << "CUSTOM[" << kind_str << "]("
            << func->name()
            << ", n=" << custom_args_.size() << ")";
        std::cout << std::setw(18) << oss.str();
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

double& operator<<(double& a, const Expression& b) {
    a = b->value();
    return a;
}

//////////////////////////

void zero_all_grad() {
    ExpressionImpl::zero_all_grad();
}

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
    : input_dim_(input_dim)
    , kind_(ImplKind::Graph) {
    // Determine name
    if (name.empty()) {
        static std::atomic<size_t> counter{0};
        size_t id = counter++;
        name_ = "custom_f_" + std::to_string(id);
    } else {
        name_ = name;
    }

    // 1. Create local input variables
    local_vars_.reserve(input_dim_);
    for (size_t i = 0; i < input_dim_; ++i) {
        local_vars_.emplace_back(Variable());
    }

    // 2. Call builder to construct expression tree
    output_ = builder(local_vars_);

    // 3. Build list of all nodes in internal expression tree
    build_nodes_list();
}

// Native type constructor (value + gradient separate)
CustomFunctionImpl::CustomFunctionImpl(size_t input_dim,
                                       NativeValueFunc value,
                                       NativeGradFunc gradient,
                                       const std::string& name)
    : input_dim_(input_dim)
    , kind_(ImplKind::Native)
    , native_value_(std::move(value))
    , native_grad_(std::move(gradient)) {
    if (!native_value_ || !native_grad_) {
        throw Nh::RuntimeException(
            "CustomFunctionImpl(Native): value and gradient must be non-null");
    }

    if (name.empty()) {
        static std::atomic<size_t> counter{0};
        size_t id = counter++;
        name_ = "native_f_" + std::to_string(id);
    } else {
        name_ = name;
    }
}

// Native type constructor (value_and_gradient combined)
CustomFunctionImpl::CustomFunctionImpl(size_t input_dim,
                                       NativeValueGradFunc value_and_gradient,
                                       const std::string& name)
    : input_dim_(input_dim)
    , kind_(ImplKind::Native)
    , native_value_grad_(std::move(value_and_gradient)) {
    if (!native_value_grad_) {
        throw Nh::RuntimeException(
            "CustomFunctionImpl(Native): value_and_gradient must be non-null");
    }

    if (name.empty()) {
        static std::atomic<size_t> counter{0};
        size_t id = counter++;
        name_ = "native_f_" + std::to_string(id);
    } else {
        name_ = name;
    }
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
    // Graph type only - early return for Native type
    if (kind_ != ImplKind::Graph) {
        return;  // Native type doesn't need this
    }

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
                                    const std::vector<double>& b) const {
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
    if (x.size() != input_dim_) {
        throw Nh::RuntimeException(
            "CustomFunctionImpl::value: input dimension mismatch");
    }

    if (kind_ == ImplKind::Graph) {
        set_inputs_and_clear(x);
        double v = output_->value();
        // Cache the value and arguments for potential reuse in eval_with_gradient
        last_args_ = x;
        last_value_ = v;
        has_cached_value_ = true;
        return v;
    } else {  // Native
        double v;
        if (native_value_) {
            v = native_value_(x);
        } else if (native_value_grad_) {
            v = native_value_grad_(x).first;
        } else {
            throw Nh::RuntimeException(
                "CustomFunctionImpl(Native)::value: no value function");
        }
        last_args_ = x;
        last_value_ = v;
        has_cached_value_ = true;
        return v;
    }
}

std::vector<double> CustomFunctionImpl::gradient(const std::vector<double>& x) {
    if (x.size() != input_dim_) {
        throw Nh::RuntimeException(
            "CustomFunctionImpl::gradient: input dimension mismatch");
    }

    if (kind_ == ImplKind::Graph) {
        set_inputs_and_clear(x);
        output_->value();
        output_->backward(1.0);

        std::vector<double> grad(input_dim_);
        for (size_t i = 0; i < input_dim_; ++i) {
            grad[i] = local_vars_[i]->gradient();
        }
        return grad;
    } else {  // Native
        std::vector<double> grad;
        if (native_grad_) {
            grad = native_grad_(x);
        } else if (native_value_grad_) {
            grad = native_value_grad_(x).second;
        } else {
            throw Nh::RuntimeException(
                "CustomFunctionImpl(Native)::gradient: no gradient function");
        }

        // Verify gradient dimension matches input dimension
        if (grad.size() != input_dim_) {
            throw Nh::RuntimeException(
                "CustomFunctionImpl(Native)::gradient: wrong gradient dimension");
        }

        return grad;
    }
}

std::pair<double, std::vector<double>>
CustomFunctionImpl::value_and_gradient(const std::vector<double>& x) {
    if (x.size() != input_dim_) {
        throw Nh::RuntimeException(
            "CustomFunctionImpl::value_and_gradient: input dimension mismatch");
    }

    if (kind_ == ImplKind::Graph) {
        set_inputs_and_clear(x);
        double v = output_->value();
        output_->backward(1.0);

        std::vector<double> grad(input_dim_);
        for (size_t i = 0; i < input_dim_; ++i) {
            grad[i] = local_vars_[i]->gradient();
        }

        last_args_ = x;
        last_value_ = v;
        has_cached_value_ = true;

        return {v, std::move(grad)};
    } else {  // Native
        double v;
        std::vector<double> g;
        if (native_value_grad_) {
            auto pair = native_value_grad_(x);
            v = pair.first;
            g = std::move(pair.second);
        } else if (native_value_ && native_grad_) {
            v = native_value_(x);
            g = native_grad_(x);
        } else {
            throw Nh::RuntimeException(
                "CustomFunctionImpl(Native)::value_and_gradient: "
                "insufficient callbacks");
        }

        last_args_ = x;
        last_value_ = v;
        has_cached_value_ = true;

        return {v, std::move(g)};
    }
}

std::pair<double, std::vector<double>>
CustomFunctionImpl::eval_with_gradient(const std::vector<double>& args_values) {
    if (args_values.size() != input_dim_) {
        throw Nh::RuntimeException(
            "CustomFunctionImpl::eval_with_gradient: size mismatch");
    }

    if (kind_ == ImplKind::Graph) {
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
    } else {  // Native
        // Native doesn't have internal state, so just call value_and_gradient
        return value_and_gradient(args_values);
    }
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
