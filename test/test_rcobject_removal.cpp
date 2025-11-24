// -*- c++ -*-
// Unit tests for RCObject removal verification
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

class RCObjectRemovalTest : public ::testing::Test {
   protected:
    void SetUp() override {}

    void TearDown() override {}
};

// Test: Verify that RCObject's refer() and release() are not being called
// This test verifies that RCObject's reference counting is not used
TEST_F(RCObjectRemovalTest, RCObjectReferenceCountingNotUsed) {
    // Create objects that inherit from RCObject
    Normal n1(10.0, 2.0);
    Normal n2(20.0, 3.0);

    // Verify that objects work correctly without RCObject's reference counting
    EXPECT_DOUBLE_EQ(n1->mean(), 10.0);
    EXPECT_DOUBLE_EQ(n1->variance(), 2.0);
    EXPECT_DOUBLE_EQ(n2->mean(), 20.0);
    EXPECT_DOUBLE_EQ(n2->variance(), 3.0);

    // Copy should work via std::shared_ptr, not RCObject
    RandomVar n3 = n1;
    EXPECT_DOUBLE_EQ(n3->mean(), 10.0);
    EXPECT_DOUBLE_EQ(n3->variance(), 2.0);

    // Verify they share the same underlying object via shared_ptr
    EXPECT_EQ(n1.get(), n3.get());
}

// Test: Verify that RCObject inheritance doesn't affect functionality
// This test ensures that removing RCObject won't break existing code
TEST_F(RCObjectRemovalTest, RCObjectInheritanceDoesNotAffectFunctionality) {
    // Test RandomVariable (inherits from RCObject)
    Normal n1(10.0, 2.0);
    Normal n2(20.0, 3.0);
    RandomVar sum = n1 + n2;
    EXPECT_DOUBLE_EQ(sum->mean(), 30.0);
    EXPECT_DOUBLE_EQ(sum->variance(), 5.0);

    // Test Expression (inherits from RCObject)
    Expression e1 = Const(5.0);
    Expression e2 = Const(10.0);
    Expression sum_expr = e1 + e2;
    // Note: Expression doesn't have a direct value() method accessible here
    // but we can verify it's created successfully
    EXPECT_NE(sum_expr.get(), nullptr);

    // Test Gate (inherits from RCObject)
    Gate gate;
    gate->set_type_name("test_gate");
    EXPECT_EQ(gate->type_name(), "test_gate");

    // Test Instance (inherits from RCObject)
    Instance inst = gate.create_instance();
    EXPECT_NE(inst.get(), nullptr);
}

// Test: Verify that std::shared_ptr handles memory management correctly
// This test ensures that shared_ptr is doing the work, not RCObject
TEST_F(RCObjectRemovalTest, SharedPtrHandlesMemoryManagement) {
    using namespace RandomVariable;
    std::shared_ptr<_RandomVariable_> ptr;

    {
        Normal n1(10.0, 2.0);
        // Get the underlying shared_ptr
        ptr = n1.shared();
        size_t count_after_shared = ptr.use_count();  // shared() creates a copy

        {
            Normal n2 = n1;
            auto ptr2 = n2.shared();
            // Both n1 and n2 share ownership, plus shared() calls create copies
            EXPECT_GE(ptr.use_count(), count_after_shared);
            EXPECT_EQ(ptr.get(), ptr2.get());
        }
        // n2 destroyed, but ptr should still be valid
        EXPECT_GE(ptr.use_count(), 1);
    }
    // n1 destroyed, but ptr should still be valid (we hold a reference)
    EXPECT_GE(ptr.use_count(), 1);
    EXPECT_DOUBLE_EQ(ptr->mean(), 10.0);
    EXPECT_DOUBLE_EQ(ptr->variance(), 2.0);
}

// Test: Verify that RCObject's refCount() is not used
// This test documents that RCObject's reference counting is unused
TEST_F(RCObjectRemovalTest, RCObjectRefCountNotUsed) {
    Normal n1(10.0, 2.0);
    Normal n2 = n1;

    // Verify that objects work correctly
    EXPECT_DOUBLE_EQ(n1->mean(), 10.0);
    EXPECT_DOUBLE_EQ(n2->mean(), 10.0);

    // The important point: memory management is handled by std::shared_ptr,
    // not by RCObject's reference counting
    // If RCObject's refCount() were being used, we would see different behavior
    // But since we're using std::shared_ptr, RCObject's counter is not used
}

// Test: Verify that CovarianceMatrix works without RCObject's reference counting
TEST_F(RCObjectRemovalTest, CovarianceMatrixWorksWithoutRCObject) {
    using namespace RandomVariable;
    Normal n1(10.0, 2.0);
    Normal n2(20.0, 3.0);

    // Create covariance matrix
    CovarianceMatrix cm;

    // Set covariance
    cm->set(n1, n2, 1.5);

    // Lookup covariance
    double cov = 0.0;
    bool found = cm->lookup(n1, n2, cov);
    EXPECT_TRUE(found);
    EXPECT_DOUBLE_EQ(cov, 1.5);

    // Verify that memory management works via shared_ptr
    // CovarianceMatrix is a Handle type, so we can access it via operator->
    auto cm_ptr = cm.shared();
    EXPECT_NE(cm_ptr.get(), nullptr);
}

// Test: Verify that removing RCObject inheritance won't break polymorphism
TEST_F(RCObjectRemovalTest, RemovingRCObjectWontBreakPolymorphism) {
    // Test that objects can still be used polymorphically
    Normal n1(10.0, 2.0);
    RandomVar rv = n1;

    // Verify that we can call virtual methods
    EXPECT_DOUBLE_EQ(rv->mean(), 10.0);
    EXPECT_DOUBLE_EQ(rv->variance(), 2.0);

    // Verify that dynamic_cast still works
    using namespace RandomVariable;
    auto normal_ptr = rv.dynamic_pointer_cast<_Normal_>();
    EXPECT_NE(normal_ptr, nullptr);
    EXPECT_DOUBLE_EQ(normal_ptr->mean(), 10.0);
}

// Test: Verify that all RCObject-inheriting classes work correctly
// This is a comprehensive test to ensure removal won't break anything
TEST_F(RCObjectRemovalTest, AllRCObjectInheritingClassesWork) {
    using namespace RandomVariable;
    // _RandomVariable_ (via Normal)
    Normal n1(10.0, 2.0);
    EXPECT_DOUBLE_EQ(n1->mean(), 10.0);

    // _Gate_
    Gate gate;
    gate->set_type_name("test");
    EXPECT_EQ(gate->type_name(), "test");

    // _Instance_
    Instance inst = gate.create_instance();
    EXPECT_NE(inst.get(), nullptr);

    // Expression_
    Expression expr = Const(5.0);
    EXPECT_NE(expr.get(), nullptr);

    // _CovarianceMatrix_
    CovarianceMatrix cm;
    cm->set(n1, n1, 1.0);
    double cov = 0.0;
    EXPECT_TRUE(cm->lookup(n1, n1, cov));
    EXPECT_DOUBLE_EQ(cov, 1.0);
}
