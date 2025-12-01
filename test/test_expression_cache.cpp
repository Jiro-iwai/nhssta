// -*- c++ -*-
// Test for Expression caching in mean_expr/var_expr
// Issue: Without caching, Expression nodes explode exponentially

#include <gtest/gtest.h>

#include "expression.hpp"
#include "normal.hpp"
#include "add.hpp"
#include "max.hpp"
#include "covariance.hpp"

using ::RandomVariable::Normal;
using ::Expression;
using ::ExpressionImpl;
using ::RandomVariable::MAX;
using ::RandomVariable::clear_cov_expr_cache;

class ExpressionCacheTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Clear any existing Expression nodes
        ExpressionImpl::zero_all_grad();
    }
    
    void TearDown() override {
        clear_cov_expr_cache();
    }
};

// Test: Calling mean_expr() twice on the same RandomVariable should return cached result
TEST_F(ExpressionCacheTest, MeanExprCached) {
    Normal a(10.0, 4.0);  // μ=10, σ²=4
    
    Expression expr1 = a->mean_expr();
    size_t after_first = ExpressionImpl::node_count();
    
    Expression expr2 = a->mean_expr();
    size_t after_second = ExpressionImpl::node_count();
    
    // Second call should not create new nodes (cached)
    EXPECT_EQ(after_first, after_second) << "mean_expr() should be cached";
    
    // Both should return the same Expression
    EXPECT_EQ(expr1->value(), expr2->value());
}

// Test: Calling var_expr() twice on the same RandomVariable should return cached result
TEST_F(ExpressionCacheTest, VarExprCached) {
    Normal a(10.0, 4.0);
    
    Expression expr1 = a->var_expr();
    size_t after_first = ExpressionImpl::node_count();
    
    Expression expr2 = a->var_expr();
    size_t after_second = ExpressionImpl::node_count();
    
    // Second call should not create new nodes (cached)
    EXPECT_EQ(after_first, after_second) << "var_expr() should be cached";
}

// Test: For OpADD, mean_expr should be cached
TEST_F(ExpressionCacheTest, OpADD_MeanExprCached) {
    Normal a(10.0, 4.0);
    Normal b(20.0, 9.0);
    ::RandomVariable::RandomVariable sum = a + b;
    
    Expression expr1 = sum->mean_expr();
    size_t after_first = ExpressionImpl::node_count();
    
    Expression expr2 = sum->mean_expr();
    size_t after_second = ExpressionImpl::node_count();
    
    EXPECT_EQ(after_first, after_second) << "OpADD::mean_expr() should be cached";
    EXPECT_DOUBLE_EQ(expr1->value(), 30.0);  // 10 + 20
}

// Test: For OpMAX, mean_expr should be cached
TEST_F(ExpressionCacheTest, OpMAX_MeanExprCached) {
    Normal a(10.0, 4.0);
    Normal b(20.0, 9.0);
    ::RandomVariable::RandomVariable m = MAX(a, b);
    
    Expression expr1 = m->mean_expr();
    size_t after_first = ExpressionImpl::node_count();
    
    Expression expr2 = m->mean_expr();
    size_t after_second = ExpressionImpl::node_count();
    
    EXPECT_EQ(after_first, after_second) << "OpMAX::mean_expr() should be cached";
}

// Test: Complex expression should have reasonable node count
TEST_F(ExpressionCacheTest, ComplexExpression_ReasonableNodeCount) {
    // Create a chain: a + b + c + d
    Normal a(10.0, 4.0);
    Normal b(20.0, 9.0);
    Normal c(30.0, 16.0);
    Normal d(40.0, 25.0);
    
    ::RandomVariable::RandomVariable ab = a + b;
    ::RandomVariable::RandomVariable abc = ab + c;
    ::RandomVariable::RandomVariable abcd = abc + d;
    
    size_t before = ExpressionImpl::node_count();
    
    // Call mean_expr and std_expr (which uses var_expr)
    Expression mean = abcd->mean_expr();
    Expression std = abcd->std_expr();
    
    size_t after = ExpressionImpl::node_count();
    size_t created = after - before;
    
    // With 4 Normal variables, we expect:
    // - 4 mu variables + 4 sigma variables = 8 leaf nodes
    // - Some intermediate nodes for operations
    // Without caching, this could explode to hundreds/thousands
    // With caching, should be < 100 nodes
    EXPECT_LT(created, 100) << "Expression node count should be reasonable with caching";
    
    std::cout << "Expression nodes created: " << created << std::endl;
}

// Test: Nested MAX operations should have reasonable node count
TEST_F(ExpressionCacheTest, NestedMAX_ReasonableNodeCount) {
    Normal a(10.0, 4.0);
    Normal b(20.0, 9.0);
    Normal c(30.0, 16.0);
    
    // MAX(MAX(a, b), c) - simulates circuit timing
    ::RandomVariable::RandomVariable m1 = MAX(a, b);
    ::RandomVariable::RandomVariable m2 = MAX(m1, c);
    
    size_t before = ExpressionImpl::node_count();
    
    Expression mean = m2->mean_expr();
    Expression std = m2->std_expr();
    
    size_t after = ExpressionImpl::node_count();
    size_t created = after - before;
    
    // With caching, this should be manageable
    EXPECT_LT(created, 200) << "Nested MAX should have reasonable node count";
    
    std::cout << "Nested MAX Expression nodes created: " << created << std::endl;
}

// Test: backward() should complete in reasonable time
TEST_F(ExpressionCacheTest, BackwardCompletesQuickly) {
    Normal a(10.0, 4.0);
    Normal b(20.0, 9.0);
    ::RandomVariable::RandomVariable sum = a + b;
    
    Expression mean = sum->mean_expr();
    Expression std = sum->std_expr();
    Expression score = mean + std;
    
    // This should complete quickly (not hang)
    ExpressionImpl::zero_all_grad();
    score->backward();
    
    // Check that gradients are set
    double grad_mu_a = a->mean_expr()->gradient();
    
    // For sum = a + b, mean(sum) = mean(a) + mean(b)
    // ∂mean(sum)/∂mean(a) = 1
    EXPECT_NEAR(grad_mu_a, 1.0, 0.01);
}

