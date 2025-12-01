/**
 * @file test_expression_cache_investigation.cpp
 * @brief Detailed investigation of Expression caching vs RandomVariable caching (Issue #188)
 */

#include <gtest/gtest.h>
#include <iostream>
#include <iomanip>
#include <unordered_map>

#include "expression.hpp"
#include "random_variable.hpp"
#include "normal.hpp"
#include "add.hpp"
#include "max.hpp"
#include "covariance.hpp"
#include "test_expression_helpers.hpp"

namespace RandomVariable {

class ExpressionCacheInvestigation : public ::testing::Test {
protected:
    void SetUp() override {
        initial_count_ = ::ExpressionImpl::node_count();
    }
    
    size_t nodes_created() const {
        return ::ExpressionImpl::node_count() - initial_count_;
    }
    
    size_t initial_count_;
};

// Test 1: RandomVariable caching works - same object returns same Expression
TEST_F(ExpressionCacheInvestigation, RandomVariable_CachingWorks) {
    std::cout << "\n=== Test 1: RandomVariable Caching ===" << std::endl;
    
    Normal a(10.0, 4.0);
    Normal b(20.0, 9.0);
    RandomVariable ab = a + b;
    
    size_t before = ::ExpressionImpl::node_count();
    Expression e1 = ab->mean_expr();
    size_t first_call = ::ExpressionImpl::node_count() - before;
    std::cout << "First mean_expr() call: " << first_call << " nodes" << std::endl;
    
    before = ::ExpressionImpl::node_count();
    Expression e2 = ab->mean_expr();
    size_t second_call = ::ExpressionImpl::node_count() - before;
    std::cout << "Second mean_expr() call (same object): " << second_call << " nodes" << std::endl;
    
    EXPECT_EQ(second_call, 0) << "RandomVariable should cache Expression";
    EXPECT_EQ(e1.get(), e2.get()) << "Should return same Expression object";
}

// Test 2: Expression-level function caching - same Expression objects
TEST_F(ExpressionCacheInvestigation, ExpressionFunction_SameObjects) {
    std::cout << "\n=== Test 2: Expression Function with Same Objects ===" << std::endl;
    
    ::Variable mu0, sigma0, mu1, sigma1, rho;
    mu0 = 5.0; sigma0 = 1.0;
    mu1 = 5.0; sigma1 = 1.0;
    rho = 0.5;
    
    size_t before = ::ExpressionImpl::node_count();
    Expression e1 = expected_prod_pos_expr(mu0, sigma0, mu1, sigma1, rho);
    size_t first_call = ::ExpressionImpl::node_count() - before;
    std::cout << "First expected_prod_pos_expr() call: " << first_call << " nodes" << std::endl;
    
    before = ::ExpressionImpl::node_count();
    Expression e2 = expected_prod_pos_expr(mu0, sigma0, mu1, sigma1, rho);
    size_t second_call = ::ExpressionImpl::node_count() - before;
    std::cout << "Second call (same Expression objects): " << second_call << " nodes" << std::endl;
    
    std::cout << "e1.get() = " << e1.get() << ", e2.get() = " << e2.get() << std::endl;
    std::cout << "Are they the same? " << (e1.get() == e2.get() ? "YES" : "NO") << std::endl;
    
    // With caching, same Expression objects should return cached result
    EXPECT_EQ(second_call, 0) << "Expression-level functions should cache with same objects";
    EXPECT_EQ(e1.get(), e2.get()) << "Should return same Expression object";
}

// Test 3: Expression-level function - different Expression objects with same values
TEST_F(ExpressionCacheInvestigation, ExpressionFunction_DifferentObjectsSameValues) {
    std::cout << "\n=== Test 3: Expression Function with Different Objects (Same Values) ===" << std::endl;
    
    ::Variable mu0_1, sigma0_1, mu1_1, sigma1_1, rho_1;
    mu0_1 = 5.0; sigma0_1 = 1.0;
    mu1_1 = 5.0; sigma1_1 = 1.0;
    rho_1 = 0.5;
    
    size_t before = ::ExpressionImpl::node_count();
    Expression e1 = expected_prod_pos_expr(mu0_1, sigma0_1, mu1_1, sigma1_1, rho_1);
    size_t first_call = ::ExpressionImpl::node_count() - before;
    std::cout << "First call: " << first_call << " nodes" << std::endl;
    
    ::Variable mu0_2, sigma0_2, mu1_2, sigma1_2, rho_2;
    mu0_2 = 5.0; sigma0_2 = 1.0;
    mu1_2 = 5.0; sigma1_2 = 1.0;
    rho_2 = 0.5;
    
    before = ::ExpressionImpl::node_count();
    Expression e2 = expected_prod_pos_expr(mu0_2, sigma0_2, mu1_2, sigma1_2, rho_2);
    size_t second_call = ::ExpressionImpl::node_count() - before;
    std::cout << "Second call (different Expression objects, same values): " << second_call << " nodes" << std::endl;
    
    EXPECT_GT(second_call, 0) << "Expression-level functions don't cache by value";
}

// Test 4: Why RandomVariable caching works - object identity
TEST_F(ExpressionCacheInvestigation, RandomVariable_ObjectIdentity) {
    std::cout << "\n=== Test 4: RandomVariable Object Identity ===" << std::endl;
    
    Normal a(10.0, 4.0);
    Normal b(20.0, 9.0);
    RandomVariable ab1 = a + b;
    RandomVariable ab2 = a + b;  // Different object, but same computation
    
    std::cout << "ab1.get() = " << ab1.get() << ", ab2.get() = " << ab2.get() << std::endl;
    std::cout << "Are they the same object? " << (ab1.get() == ab2.get() ? "YES" : "NO") << std::endl;
    
    // RandomVariable caching works because:
    // 1. Each RandomVariable object has its own cached Expression
    // 2. Same RandomVariable object → same cached Expression
    // 3. Different RandomVariable objects → different cached Expressions (even if computation is same)
    
    Expression e1 = ab1->mean_expr();
    Expression e2 = ab2->mean_expr();
    
    std::cout << "e1.get() = " << e1.get() << ", e2.get() = " << e2.get() << std::endl;
    std::cout << "Are Expressions the same? " << (e1.get() == e2.get() ? "YES" : "NO") << std::endl;
    
    // Different RandomVariable objects → different cached Expressions
    EXPECT_NE(e1.get(), e2.get()) << "Different RandomVariable objects have different cached Expressions";
}

// Test 5: Expression function called from RandomVariable context
TEST_F(ExpressionCacheInvestigation, ExpressionFunction_FromRandomVariable) {
    std::cout << "\n=== Test 5: Expression Function Called from RandomVariable ===" << std::endl;
    
    Normal d0(5.0, 1.0);
    Normal d1(5.0, 1.0);
    RandomVariable max0_d0 = MAX0(d0);
    RandomVariable max0_d1 = MAX0(d1);
    
    // This simulates what happens in cov_max0_max0_expr_impl
    Expression mu0 = d0->mean_expr();
    Expression sigma0 = d0->std_expr();
    Expression mu1 = d1->mean_expr();
    Expression sigma1 = d1->std_expr();
    
    std::cout << "mu0.get() = " << mu0.get() << ", sigma0.get() = " << sigma0.get() << std::endl;
    std::cout << "mu1.get() = " << mu1.get() << ", sigma1.get() = " << sigma1.get() << std::endl;
    
    // Get rho from covariance
    Expression cov_d0d1 = cov_expr(d0, d1);
    Expression rho = cov_d0d1 / (sigma0 * sigma1);
    
    size_t before = ::ExpressionImpl::node_count();
    Expression e1 = expected_prod_pos_expr(mu0, sigma0, mu1, sigma1, rho);
    size_t first_call = ::ExpressionImpl::node_count() - before;
    std::cout << "First expected_prod_pos_expr() call: " << first_call << " nodes" << std::endl;
    
    // Call again with same Expression objects
    before = ::ExpressionImpl::node_count();
    Expression e2 = expected_prod_pos_expr(mu0, sigma0, mu1, sigma1, rho);
    size_t second_call = ::ExpressionImpl::node_count() - before;
    std::cout << "Second call (same Expression objects): " << second_call << " nodes" << std::endl;
    
    std::cout << "e1.get() = " << e1.get() << ", e2.get() = " << e2.get() << std::endl;
    std::cout << "Are they the same? " << (e1.get() == e2.get() ? "YES" : "NO") << std::endl;
    
    // With caching, same Expression objects should return cached result
    EXPECT_EQ(second_call, 0) << "Expression-level functions should cache with same objects";
    EXPECT_EQ(e1.get(), e2.get()) << "Should return same Expression object";
}

// Test 6: Breakdown of expected_prod_pos_expr node creation
TEST_F(ExpressionCacheInvestigation, ExpectedProdPos_Breakdown) {
    std::cout << "\n=== Test 6: Breakdown of expected_prod_pos_expr ===" << std::endl;
    
    ::Variable mu0, sigma0, mu1, sigma1, rho;
    mu0 = 5.0; sigma0 = 1.0;
    mu1 = 5.0; sigma1 = 1.0;
    rho = 0.5;
    
    size_t before = ::ExpressionImpl::node_count();
    
    Expression a0 = mu0 / sigma0;
    Expression a1 = mu1 / sigma1;
    Expression one_minus_rho2 = Const(1.0) - rho * rho;
    Expression sqrt_one_minus_rho2 = sqrt(one_minus_rho2);
    Expression Phi2_a0_a1 = Phi2_expr_test(a0, a1, rho);
    Expression phi_a0 = phi_expr(a0);
    Expression phi_a1 = phi_expr(a1);
    Expression Phi_cond_0 = Phi_expr((a0 - rho * a1) / sqrt_one_minus_rho2);
    Expression Phi_cond_1 = Phi_expr((a1 - rho * a0) / sqrt_one_minus_rho2);
    Expression phi2_a0_a1 = phi2_expr_test(a0, a1, rho);
    
    size_t intermediate = ::ExpressionImpl::node_count() - before;
    std::cout << "Intermediate expressions: " << intermediate << " nodes" << std::endl;
    
    Expression term1 = mu0 * mu1 * Phi2_a0_a1;
    Expression term2 = mu0 * sigma1 * phi_a1 * Phi_cond_0;
    Expression term3 = mu1 * sigma0 * phi_a0 * Phi_cond_1;
    Expression term4 = sigma0 * sigma1 * (rho * Phi2_a0_a1 + one_minus_rho2 * phi2_a0_a1);
    Expression result = term1 + term2 + term3 + term4;
    
    size_t total = ::ExpressionImpl::node_count() - before;
    std::cout << "Total nodes: " << total << std::endl;
    
    // Call again
    before = ::ExpressionImpl::node_count();
    Expression result2 = expected_prod_pos_expr(mu0, sigma0, mu1, sigma1, rho);
    size_t second_call = ::ExpressionImpl::node_count() - before;
    std::cout << "Second call (via function): " << second_call << " nodes" << std::endl;
    
    // Note: second_call may be less than total due to optimizations (e.g., Const(1.0) -> one)
    // The important thing is that the function works correctly, not that it creates exactly the same number of nodes
    EXPECT_LE(second_call, total) << "Second call should create same or fewer nodes (due to optimizations)";
    EXPECT_GT(second_call, 0) << "Second call should create some nodes";
}

// Test 7: Comparison with cov_expr caching
TEST_F(ExpressionCacheInvestigation, CovExpr_Caching) {
    std::cout << "\n=== Test 7: cov_expr Caching ===" << std::endl;
    
    Normal a(10.0, 4.0);
    Normal b(20.0, 9.0);
    RandomVariable max0_a = MAX0(a);
    RandomVariable max0_b = MAX0(b);
    
    size_t before = ::ExpressionImpl::node_count();
    Expression e1 = cov_expr(max0_a, max0_b);
    size_t first_call = ::ExpressionImpl::node_count() - before;
    std::cout << "First cov_expr() call: " << first_call << " nodes" << std::endl;
    
    before = ::ExpressionImpl::node_count();
    Expression e2 = cov_expr(max0_a, max0_b);
    size_t second_call = ::ExpressionImpl::node_count() - before;
    std::cout << "Second cov_expr() call (same RandomVariable objects): " << second_call << " nodes" << std::endl;
    
    std::cout << "e1.get() = " << e1.get() << ", e2.get() = " << e2.get() << std::endl;
    std::cout << "Are they the same? " << (e1.get() == e2.get() ? "YES" : "NO") << std::endl;
    
    EXPECT_EQ(second_call, 0) << "cov_expr should cache";
    EXPECT_EQ(e1.get(), e2.get()) << "Should return same Expression object";
}

}  // namespace RandomVariable

