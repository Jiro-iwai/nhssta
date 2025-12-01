/**
 * @file test_expression_node_count.cpp
 * @brief Investigation of Expression node count (Issue #188)
 */

#include <gtest/gtest.h>
#include <iostream>
#include <iomanip>

#include "expression.hpp"
#include "test_expression_helpers.hpp"
#include "random_variable.hpp"
#include "normal.hpp"
#include "add.hpp"
#include "sub.hpp"
#include "max.hpp"
#include "covariance.hpp"

namespace RandomVariable {

class ExpressionNodeCountTest : public ::testing::Test {
protected:
    void SetUp() override {
        initial_count_ = ::ExpressionImpl::node_count();
    }
    
    size_t nodes_created() const {
        return ::ExpressionImpl::node_count() - initial_count_;
    }
    
    size_t initial_count_;
};

TEST_F(ExpressionNodeCountTest, SimpleArithmetic) {
    std::cout << "\n=== Simple Arithmetic ===" << std::endl;
    
    ::Variable a, b, c, d;
    a = 1.0; b = 2.0; c = 3.0; d = 4.0;
    
    size_t before = ::ExpressionImpl::node_count();
    ::Expression e1 = a + b;
    std::cout << "a + b: " << (::ExpressionImpl::node_count() - before) << " nodes" << std::endl;
    
    before = ::ExpressionImpl::node_count();
    ::Expression e2 = a + b + c;
    std::cout << "a + b + c: " << (::ExpressionImpl::node_count() - before) << " nodes" << std::endl;
    
    before = ::ExpressionImpl::node_count();
    ::Expression e3 = a + b + c + d;
    std::cout << "a + b + c + d: " << (::ExpressionImpl::node_count() - before) << " nodes" << std::endl;
    
    before = ::ExpressionImpl::node_count();
    ::Expression e4 = a * b;
    std::cout << "a * b: " << (::ExpressionImpl::node_count() - before) << " nodes" << std::endl;
    
    before = ::ExpressionImpl::node_count();
    ::Expression e5 = a / b;
    std::cout << "a / b: " << (::ExpressionImpl::node_count() - before) << " nodes" << std::endl;
}

TEST_F(ExpressionNodeCountTest, MathFunctions) {
    std::cout << "\n=== Math Functions ===" << std::endl;
    
    ::Variable x;
    x = 1.0;
    
    size_t before = ::ExpressionImpl::node_count();
    ::Expression e1 = exp(x);
    std::cout << "exp(x): " << (::ExpressionImpl::node_count() - before) << " nodes" << std::endl;
    
    before = ::ExpressionImpl::node_count();
    ::Expression e2 = log(x);
    std::cout << "log(x): " << (::ExpressionImpl::node_count() - before) << " nodes" << std::endl;
    
    before = ::ExpressionImpl::node_count();
    ::Expression e3 = sqrt(x);
    std::cout << "sqrt(x): " << (::ExpressionImpl::node_count() - before) << " nodes" << std::endl;
    
    before = ::ExpressionImpl::node_count();
    ::Expression e4 = Phi_expr(x);
    std::cout << "Phi(x): " << (::ExpressionImpl::node_count() - before) << " nodes" << std::endl;
    
    before = ::ExpressionImpl::node_count();
    ::Expression e5 = phi_expr(x);
    std::cout << "phi(x): " << (::ExpressionImpl::node_count() - before) << " nodes" << std::endl;
}

TEST_F(ExpressionNodeCountTest, Max0Expr) {
    std::cout << "\n=== max0_mean_expr ===" << std::endl;
    
    ::Variable mu, sigma;
    mu = 5.0;
    sigma = 1.0;
    
    size_t before = ::ExpressionImpl::node_count();
    ::Expression e = max0_mean_expr(mu, sigma);
    std::cout << "max0_mean_expr(mu, sigma): " << (::ExpressionImpl::node_count() - before) << " nodes" << std::endl;
}

TEST_F(ExpressionNodeCountTest, Phi2Expr) {
    std::cout << "\n=== Phi2_expr_test ===" << std::endl;
    
    ::Variable a0, a1, rho;
    a0 = 1.0;
    a1 = 1.0;
    rho = 0.5;
    
    size_t before = ::ExpressionImpl::node_count();
    ::Expression e = Phi2_expr_test(a0, a1, rho);
    std::cout << "Phi2_expr_test(a0, a1, rho): " << (::ExpressionImpl::node_count() - before) << " nodes" << std::endl;
}

TEST_F(ExpressionNodeCountTest, ExpectedProdPosExpr) {
    std::cout << "\n=== expected_prod_pos_expr ===" << std::endl;
    
    ::Variable mu0, sigma0, mu1, sigma1, rho;
    mu0 = 5.0; sigma0 = 1.0;
    mu1 = 5.0; sigma1 = 1.0;
    rho = 0.5;
    
    size_t before = ::ExpressionImpl::node_count();
    ::Expression e = expected_prod_pos_expr(mu0, sigma0, mu1, sigma1, rho);
    std::cout << "expected_prod_pos_expr: " << (::ExpressionImpl::node_count() - before) << " nodes" << std::endl;
}

TEST_F(ExpressionNodeCountTest, CovMax0Max0Expr) {
    std::cout << "\n=== cov_max0_max0_expr_test ===" << std::endl;
    
    ::Variable mu0, sigma0, mu1, sigma1, rho;
    mu0 = 5.0; sigma0 = 1.0;
    mu1 = 5.0; sigma1 = 1.0;
    rho = 0.5;
    
    size_t before = ::ExpressionImpl::node_count();
    ::Expression e = cov_max0_max0_expr_test(mu0, sigma0, mu1, sigma1, rho);
    std::cout << "cov_max0_max0_expr_test: " << (::ExpressionImpl::node_count() - before) << " nodes" << std::endl;
}

// Note: RandomVariable tests commented out due to crash during investigation
// Will investigate separately

TEST_F(ExpressionNodeCountTest, MultipleExpectedProdPos) {
    std::cout << "\n=== Multiple expected_prod_pos_expr calls ===" << std::endl;
    
    ::Variable mu0, sigma0, mu1, sigma1, rho;
    mu0 = 5.0; sigma0 = 1.0;
    mu1 = 5.0; sigma1 = 1.0;
    rho = 0.5;
    
    size_t before = ::ExpressionImpl::node_count();
    ::Expression e1 = expected_prod_pos_expr(mu0, sigma0, mu1, sigma1, rho);
    size_t first_call = ::ExpressionImpl::node_count() - before;
    std::cout << "First call: " << first_call << " nodes" << std::endl;
    
    before = ::ExpressionImpl::node_count();
    ::Expression e2 = expected_prod_pos_expr(mu0, sigma0, mu1, sigma1, rho);
    size_t second_call = ::ExpressionImpl::node_count() - before;
    std::cout << "Second call (same params): " << second_call << " nodes" << std::endl;
    
    ::Variable mu2, sigma2;
    mu2 = 3.0; sigma2 = 2.0;
    before = ::ExpressionImpl::node_count();
    ::Expression e3 = expected_prod_pos_expr(mu0, sigma0, mu2, sigma2, rho);
    size_t third_call = ::ExpressionImpl::node_count() - before;
    std::cout << "Third call (diff params): " << third_call << " nodes" << std::endl;
}

TEST_F(ExpressionNodeCountTest, Max0MeanExprBreakdown) {
    std::cout << "\n=== max0_mean_expr breakdown ===" << std::endl;
    
    ::Variable mu, sigma;
    mu = 5.0;
    sigma = 1.0;
    
    // a = -mu/sigma
    size_t before = ::ExpressionImpl::node_count();
    ::Expression neg_mu = -mu;
    std::cout << "-mu: " << (::ExpressionImpl::node_count() - before) << " nodes" << std::endl;
    
    before = ::ExpressionImpl::node_count();
    ::Expression a = neg_mu / sigma;
    std::cout << "a = -mu/sigma: " << (::ExpressionImpl::node_count() - before) << " nodes" << std::endl;
    
    before = ::ExpressionImpl::node_count();
    ::Expression phi_a = phi_expr(a);
    std::cout << "phi(a): " << (::ExpressionImpl::node_count() - before) << " nodes" << std::endl;
    
    before = ::ExpressionImpl::node_count();
    ::Expression Phi_a = Phi_expr(a);
    std::cout << "Phi(a): " << (::ExpressionImpl::node_count() - before) << " nodes" << std::endl;
    
    // Full max0_mean_expr
    before = ::ExpressionImpl::node_count();
    ::Expression full = max0_mean_expr(mu, sigma);
    std::cout << "max0_mean_expr total: " << (::ExpressionImpl::node_count() - before) << " nodes" << std::endl;
}

TEST_F(ExpressionNodeCountTest, TotalNodeCount) {
    std::cout << "\n=== Total Node Count ===" << std::endl;
    std::cout << "Total nodes in eTbl_: " << ::ExpressionImpl::node_count() << std::endl;
}

}  // namespace RandomVariable
