// -*- c++ -*-
// Author: Jiro Iwai

#include "expression.hpp"
#include "util_numerical.hpp"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <fstream>
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

// Debug flags for backward() tracing
static bool DEBUG_BACKWARD = false;
static std::ofstream* DEBUG_LOG_FILE = nullptr;
static int DEBUG_NODE_COUNT = 0;

// Enable/disable debug output for backward()
void ExpressionImpl::enable_backward_debug(bool enable, const std::string& log_file) {
    DEBUG_BACKWARD = enable;
    DEBUG_NODE_COUNT = 0;
    if (enable) {
        delete DEBUG_LOG_FILE;
        DEBUG_LOG_FILE = new std::ofstream(log_file);
    } else {
        if (DEBUG_LOG_FILE) {
            DEBUG_LOG_FILE->close();
        }
        delete DEBUG_LOG_FILE;
        DEBUG_LOG_FILE = nullptr;
    }
}

std::ostream& get_debug_log() {
    if (DEBUG_BACKWARD && DEBUG_LOG_FILE && DEBUG_LOG_FILE->is_open()) {
        return *DEBUG_LOG_FILE;
    }
    return std::cout;
}

static std::string op_str(ExpressionImpl::Op op) {
    switch (op) {
        case ExpressionImpl::CONST: return "CONST";
        case ExpressionImpl::PARAM: return "PARAM";
        case ExpressionImpl::PLUS: return "PLUS";
        case ExpressionImpl::MINUS: return "MINUS";
        case ExpressionImpl::MUL: return "MUL";
        case ExpressionImpl::DIV: return "DIV";
        case ExpressionImpl::POWER: return "POWER";
        case ExpressionImpl::EXP: return "EXP";
        case ExpressionImpl::LOG: return "LOG";
        case ExpressionImpl::ERF: return "ERF";
        case ExpressionImpl::SQRT: return "SQRT";
        case ExpressionImpl::CUSTOM_FUNCTION: return "CUSTOM_FUNCTION";
        default: return "UNKNOWN";
    }
}

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
    , right_(right) {
    eTbl().insert(this);
    if (left_ != null) {
        left_->add_root(this);
    }
    if (right_ != null) {
        right_->add_root(this);
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

        // Binary operations
        if (left() != null && right() != null) {
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
        if (DEBUG_BACKWARD) {
            get_debug_log() << "  Leaf node " << id_ << " (op=" << op_str(op_) << ") - no propagation\n";
        }
        return;
    }
    
    double upstream = gradient_;
    
    if (DEBUG_BACKWARD) {
        DEBUG_NODE_COUNT++;
        get_debug_log() << "\n=== propagate_gradient() #" << DEBUG_NODE_COUNT << " ===\n";
        get_debug_log() << "  Node[" << id_ << "] (op=" << op_str(op_) << ")\n";
        get_debug_log() << "  Upstream gradient: " << upstream << "\n";
        get_debug_log() << "  Value: " << value_ << "\n";
    }
    
    // Check CUSTOM_FUNCTION operation first
    if (op_ == CUSTOM_FUNCTION) {
        if (DEBUG_BACKWARD) {
            get_debug_log() << "  CUSTOM_FUNCTION node\n";
            get_debug_log() << "  Args count: " << custom_args_.size() << "\n";
        }
        
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

        if (DEBUG_BACKWARD) {
            get_debug_log() << "  Gradients to propagate:\n";
        }
        
        for (size_t i = 0; i < n; ++i) {
            double contrib = upstream * grad_vec[i];
            double old_grad = custom_args_[i]->gradient_;
            custom_args_[i]->gradient_ += contrib;
            
            if (DEBUG_BACKWARD) {
                get_debug_log() << "    arg[" << i << "]: upstream=" << upstream 
                               << " × grad[" << i << "]=" << grad_vec[i] 
                               << " = contrib=" << contrib << "\n";
                get_debug_log() << "    arg[" << i << "] gradient: " << old_grad 
                               << " -> " << custom_args_[i]->gradient_ << "\n";
                if (contrib < -1e-10) {
                    get_debug_log() << "    ⚠️  NEGATIVE GRADIENT CONTRIBUTION!\n";
                }
            }
        }
        return;
    }
    
    // Binary operations
    if (left() != null && right() != null) {
        double l = left()->value();
        double r = right()->value();
        
        double left_grad = 0.0;
        double right_grad = 0.0;

        if (op_ == PLUS) {
            left_grad = upstream;
            right_grad = upstream;
            left()->gradient_ += upstream;
            right()->gradient_ += upstream;
        } else if (op_ == MINUS) {
            left_grad = upstream;
            right_grad = -upstream;
            left()->gradient_ += upstream;
            right()->gradient_ += -upstream;
        } else if (op_ == MUL) {
            left_grad = upstream * r;
            right_grad = upstream * l;
            left()->gradient_ += upstream * r;
            right()->gradient_ += upstream * l;
        } else if (op_ == DIV) {
            left_grad = upstream / r;
            right_grad = -upstream * l / (r * r);
            left()->gradient_ += upstream / r;
            right()->gradient_ += -upstream * l / (r * r);
        } else if (op_ == POWER) {
            double f_val = value();
            left_grad = upstream * r * std::pow(l, r - 1);
            left()->gradient_ += upstream * r * std::pow(l, r - 1);
            if (l > 0) {
                right_grad = upstream * f_val * std::log(l);
                right()->gradient_ += upstream * f_val * std::log(l);
            }
        }
        
        if (DEBUG_BACKWARD) {
            get_debug_log() << "  Binary operation: " << op_str(op_) << "\n";
            get_debug_log() << "  Left: Node[" << left()->id() << "] (value=" << l << ")\n";
            get_debug_log() << "  Right: Node[" << right()->id() << "] (value=" << r << ")\n";
            get_debug_log() << "  Propagating to left: " << left_grad << "\n";
            get_debug_log() << "  Propagating to right: " << right_grad << "\n";
            if (left_grad < -1e-10) {
                get_debug_log() << "  ⚠️  NEGATIVE LEFT GRADIENT!\n";
            }
            if (right_grad < -1e-10) {
                get_debug_log() << "  ⚠️  NEGATIVE RIGHT GRADIENT!\n";
            }
            get_debug_log() << "  Left gradient after: " << left()->gradient_ << "\n";
            get_debug_log() << "  Right gradient after: " << right()->gradient_ << "\n";
        }
    } else if (left() != null) {
        // Unary operations
        double l = left()->value();
        double left_grad = 0.0;

        if (op_ == EXP) {
            left_grad = upstream * value();
            left()->gradient_ += upstream * value();
        } else if (op_ == LOG) {
            left_grad = upstream / l;
            left()->gradient_ += upstream / l;
        } else if (op_ == ERF) {
            static constexpr double TWO_OVER_SQRT_PI = 1.1283791670955126;
            left_grad = upstream * TWO_OVER_SQRT_PI * std::exp(-l * l);
            left()->gradient_ += upstream * TWO_OVER_SQRT_PI * std::exp(-l * l);
        } else if (op_ == SQRT) {
            double sqrt_l = value();
            left_grad = upstream / (2.0 * sqrt_l);
            left()->gradient_ += upstream / (2.0 * sqrt_l);
        }
        
        if (DEBUG_BACKWARD) {
            get_debug_log() << "  Unary operation: " << op_str(op_) << "\n";
            get_debug_log() << "  Left: Node[" << left()->id() << "] (value=" << l << ")\n";
            get_debug_log() << "  Propagating to left: " << left_grad << "\n";
            if (left_grad < -1e-10) {
                get_debug_log() << "  ⚠️  NEGATIVE LEFT GRADIENT!\n";
            }
            get_debug_log() << "  Left gradient after: " << left()->gradient_ << "\n";
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
    }
    
    // Add this node after children (reverse topological order)
    order.push_back(node);
}

void ExpressionImpl::backward(double upstream) {
    if (DEBUG_BACKWARD) {
        get_debug_log() << "\n" << std::string(80, '=') << "\n";
        get_debug_log() << "backward() called with upstream = " << upstream << "\n";
        get_debug_log() << "Root node: Node[" << id_ << "] (op=" << op_str(op_) << ")\n";
        get_debug_log() << std::string(80, '=') << "\n";
        DEBUG_NODE_COUNT = 0;
    }
    
    // Build topological order (children before parents)
    std::set<ExpressionImpl*> visited;
    std::vector<ExpressionImpl*> topo_order;
    topo_sort_dfs(this, visited, topo_order);
    
    if (DEBUG_BACKWARD) {
        get_debug_log() << "\nTopological order (" << topo_order.size() << " nodes):\n";
        for (size_t i = 0; i < topo_order.size(); ++i) {
            get_debug_log() << "  [" << i << "] Node[" << topo_order[i]->id() 
                           << "] (op=" << op_str(topo_order[i]->op()) << ")\n";
        }
    }
    
    // Set initial gradient at the root
    gradient_ += upstream;
    is_gradient_set_ = true;
    
    if (DEBUG_BACKWARD) {
        get_debug_log() << "\nInitial gradient at root: " << gradient_ << "\n";
        get_debug_log() << "\nProcessing nodes in reverse topological order:\n";
    }
    
    // Process in reverse order (parents before children)
    // This ensures all upstream gradients are accumulated before propagation
    for (auto it = topo_order.rbegin(); it != topo_order.rend(); ++it) {
        (*it)->propagate_gradient();
        (*it)->is_gradient_set_ = true;
    }
    
    if (DEBUG_BACKWARD) {
        get_debug_log() << "\n" << std::string(80, '=') << "\n";
        get_debug_log() << "backward() completed\n";
        get_debug_log() << std::string(80, '=') << "\n\n";
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
        local_vars_.emplace_back();
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
    }
}

void CustomFunctionImpl::set_inputs_and_clear(const std::vector<double>& x) {
    // Graph type only - early return for Native type
    // Native type functions don't have an internal expression tree to manage
    if (kind_ != ImplKind::Graph) {
        return;  // Native type doesn't need this
    }

    // Validate input size matches expected dimension
    if (x.size() != input_dim_) {
        throw Nh::RuntimeException(
            "CustomFunctionImpl::set_inputs_and_clear: size mismatch");
    }

    // 1. Clear value cache and gradients in internal expression tree
    //    This ensures that when new input values are set, all cached computations
    //    and gradients from previous evaluations are invalidated
    for (auto* node : nodes_) {
        // 1.1. Clear gradients for all nodes
        //      Gradients accumulate during backward pass and must be reset
        //      before the next forward evaluation
        node->zero_grad();

        // 1.2. Check if this node is a local input variable
        //      Local input variables directly overwrite their value in step 2,
        //      so cache clearing should be done from upstream (root) side.
        //      Skipping them here avoids unnecessary work.
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

        // 1.3. CONST nodes are globally shared across all expressions,
        //      so we must not modify their value_ to avoid side effects
        if (node->op() == ExpressionImpl::CONST) {
            continue;
        }

        // 1.4. Clear cache for all other nodes unconditionally.
        //      unset_value() already has a guard: if (!is_set_value()) return;
        //      so we don't need to check is_set_value() before calling it
        node->unset_value();
    }

    // 2. Finally, set new input values to local input variables
    //    This must be done after clearing caches to ensure the new values
    //    trigger recomputation of dependent nodes during the next evaluation
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
    }
    // Native
    double v = 0.0;
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
    }
    // Native
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
    }
    // Native
    double v = 0.0;
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
    }
    // Native doesn't have internal state, so just call value_and_gradient
    return value_and_gradient(args_values);
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
