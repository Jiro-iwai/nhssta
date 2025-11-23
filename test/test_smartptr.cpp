#include <gtest/gtest.h>
#include "../src/normal.hpp"
#include "../src/random_variable.hpp"
#include "../src/add.hpp"
#include "../src/gate.hpp"
#include <memory>

using RandomVar = RandomVariable::RandomVariable;
using Normal = RandomVariable::Normal;
using namespace Nh;

class SmartPtrTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Setup code if needed
    }

    void TearDown() override {
        // Cleanup code if needed
    }
};

// Test SmartPtr basic operations with Normal
TEST_F(SmartPtrTest, NormalSmartPtrBasicOperations) {
    Normal n1(10.0, 4.0);
    Normal n2 = n1; // Copy constructor
    
    // Both should have same values
    EXPECT_DOUBLE_EQ(n1->mean(), n2->mean());
    EXPECT_DOUBLE_EQ(n1->variance(), n2->variance());
    
    // They should share the same underlying object (reference counting)
    EXPECT_EQ(&(*n1), &(*n2));
}

// Test SmartPtr assignment
TEST_F(SmartPtrTest, NormalSmartPtrAssignment) {
    Normal n1(10.0, 4.0);
    Normal n2(20.0, 9.0);
    
    n2 = n1; // Assignment
    
    EXPECT_DOUBLE_EQ(n1->mean(), n2->mean());
    EXPECT_DOUBLE_EQ(n1->variance(), n2->variance());
}

// Test SmartPtr with addition operation
TEST_F(SmartPtrTest, AdditionSmartPtrLifecycle) {
    Normal a(10.0, 4.0);
    Normal b(5.0, 1.0);
    
    RandomVar sum = a + b;
    
    // Sum should be valid
    EXPECT_DOUBLE_EQ(sum->mean(), 15.0);
    EXPECT_DOUBLE_EQ(sum->variance(), 5.0);
    
    // Original variables should still be valid
    EXPECT_DOUBLE_EQ(a->mean(), 10.0);
    EXPECT_DOUBLE_EQ(b->mean(), 5.0);
}

// Test SmartPtr reference counting with multiple references
TEST_F(SmartPtrTest, ReferenceCounting) {
    Normal n1(10.0, 4.0);
    {
        Normal n2 = n1;
        Normal n3 = n1;
        
        // All should point to same object
        EXPECT_EQ(&(*n1), &(*n2));
        EXPECT_EQ(&(*n1), &(*n3));
    }
    // n2 and n3 destroyed, but n1 should still be valid
    EXPECT_DOUBLE_EQ(n1->mean(), 10.0);
}

// Test Gate SmartPtr lifecycle
TEST_F(SmartPtrTest, GateSmartPtrLifecycle) {
    Gate gate;
    gate->set_type_name("test_gate");
    
    EXPECT_EQ(gate->type_name(), "test_gate");
    
    // Create instance
    Instance inst = gate.create_instance();
    EXPECT_FALSE(inst->name().empty());
}

// Test SmartPtr comparison operators
TEST_F(SmartPtrTest, SmartPtrComparison) {
    Normal n1(10.0, 4.0);
    Normal n2(10.0, 4.0);
    Normal n3 = n1;
    
    // n1 and n3 should be equal (same object)
    EXPECT_EQ(n1, n3);
    
    // n1 and n2 might not be equal (different objects)
    // This depends on implementation
}

// Test SmartPtr with chained operations
TEST_F(SmartPtrTest, ChainedOperationsLifecycle) {
    Normal a(1.0, 1.0);
    Normal b(2.0, 1.0);
    Normal c(3.0, 1.0);
    
    RandomVar sum1 = a + b;
    RandomVar sum2 = sum1 + c;
    
    // All should be valid
    EXPECT_DOUBLE_EQ(sum2->mean(), 6.0);
    EXPECT_DOUBLE_EQ(a->mean(), 1.0);
    EXPECT_DOUBLE_EQ(b->mean(), 2.0);
    EXPECT_DOUBLE_EQ(c->mean(), 3.0);
}

// Test that SmartPtr properly manages memory (no leaks)
// This is a basic sanity check - full memory leak detection requires valgrind/asan
TEST_F(SmartPtrTest, MemoryManagement) {
    for (int i = 0; i < 100; ++i) {
        Normal n(static_cast<double>(i), 1.0);
        RandomVar sum = n + n;
        EXPECT_DOUBLE_EQ(sum->mean(), static_cast<double>(i * 2));
    }
    // If we get here without crashing, basic memory management works
}

