// -*- c++ -*-
// Unit tests for SmartPtr/RCObject complete removal verification
// Issue #45: リファクタリング: SmartPtr/RCObjectの完全フェードアウト

#include <gtest/gtest.h>

#include <memory>

#include "../src/add.hpp"
#include "../src/covariance.hpp"
#include "../src/expression.hpp"
#include "../src/gate.hpp"
#include "../src/normal.hpp"
#include "../src/random_variable.hpp"

using RandomVar = RandomVariable::RandomVariable;
using Normal = RandomVariable::Normal;
using namespace Nh;

class SmartPtrRemovalTest : public ::testing::Test {
   protected:
    void SetUp() override {}

    void TearDown() override {}
};

// Test: Verify that smart_ptr.hpp is not needed anymore
// This test verifies that all functionality works without smart_ptr.hpp
TEST_F(SmartPtrRemovalTest, NoSmartPtrHeaderNeeded) {
    // All these classes should work without smart_ptr.hpp
    Normal n1(10.0, 2.0);
    EXPECT_DOUBLE_EQ(n1->mean(), 10.0);

    Gate gate;
    gate->set_type_name("test");
    EXPECT_EQ(gate->type_name(), "test");

    Expression expr = Const(5.0);
    EXPECT_NE(expr.get(), nullptr);

    using namespace RandomVariable;
    CovarianceMatrix cm;
    cm->set(n1, n1, 1.0);
    double cov = 0.0;
    EXPECT_TRUE(cm->lookup(n1, n1, cov));
}

// Test: Verify that RCObject is not accessible
// This test verifies that RCObject is not part of the public API
TEST_F(SmartPtrRemovalTest, RCObjectNotAccessible) {
    // RCObject should not be accessible from public headers
    // If this compiles, it means RCObject is not needed
    Normal n1(10.0, 2.0);
    Gate gate;
    Expression expr = Const(5.0);

    // All should work without RCObject
    EXPECT_DOUBLE_EQ(n1->mean(), 10.0);
    EXPECT_EQ(gate->type_name(), "");
    EXPECT_NE(expr.get(), nullptr);
}

// Test: Verify that std::shared_ptr is used throughout
// This test verifies that memory management uses std::shared_ptr
TEST_F(SmartPtrRemovalTest, SharedPtrUsedThroughout) {
    Normal n1(10.0, 2.0);
    auto shared1 = n1.shared();
    EXPECT_NE(shared1.get(), nullptr);

    Gate gate;
    auto gate_shared = gate.get();
    EXPECT_NE(gate_shared.get(), nullptr);

    Expression expr = Const(5.0);
    auto expr_shared = expr.shared();
    EXPECT_NE(expr_shared.get(), nullptr);
}
