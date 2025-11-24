#include <gtest/gtest.h>

#include <memory>

#include "../src/add.hpp"
#include "../src/gate.hpp"
#include "../src/max.hpp"
#include "../src/normal.hpp"

using RandomVar = RandomVariable::RandomVariable;
using Normal = RandomVariable::Normal;
using namespace Nh;

class GateTest : public ::testing::Test {
   protected:
    void SetUp() override {
        // Common setup code
    }

    void TearDown() override {
        // Common teardown code
    }
};

// Test: Gate creation with default constructor
TEST_F(GateTest, DefaultConstructor) {
    Gate gate;
    EXPECT_TRUE(gate.get() != nullptr);
    EXPECT_TRUE(gate->type_name().empty());
}

// Test: Gate creation with type name
TEST_F(GateTest, ConstructorWithTypeName) {
    auto body = std::make_shared<_Gate_>("and2");
    Gate gate(body);
    EXPECT_EQ(gate->type_name(), "and2");
}

// Test: Set and get type name
TEST_F(GateTest, SetAndGetTypeName) {
    Gate gate;
    gate->set_type_name("or2");
    EXPECT_EQ(gate->type_name(), "or2");

    gate->set_type_name("nand2");
    EXPECT_EQ(gate->type_name(), "nand2");
}

// Test: Set delay
TEST_F(GateTest, SetDelay) {
    Gate gate;
    gate->set_type_name("test_gate");

    Normal delay1(10.0, 2.0);
    gate->set_delay("a", "y", delay1);

    const Normal& retrieved = gate->delay("a", "y");
    EXPECT_DOUBLE_EQ(retrieved->mean(), 10.0);
    EXPECT_DOUBLE_EQ(retrieved->variance(), 2.0);
}

// Test: Set multiple delays
TEST_F(GateTest, SetMultipleDelays) {
    Gate gate;
    gate->set_type_name("test_gate");

    Normal delay1(10.0, 2.0);
    Normal delay2(15.0, 3.0);

    gate->set_delay("a", "y", delay1);
    gate->set_delay("b", "y", delay2);

    const Normal& d1 = gate->delay("a", "y");
    const Normal& d2 = gate->delay("b", "y");

    EXPECT_DOUBLE_EQ(d1->mean(), 10.0);
    EXPECT_DOUBLE_EQ(d2->mean(), 15.0);
}

// Test: Get delay with default output pin
TEST_F(GateTest, GetDelayWithDefaultOutput) {
    Gate gate;
    gate->set_type_name("test_gate");

    Normal delay1(10.0, 2.0);
    gate->set_delay("a", "y", delay1);

    const Normal& retrieved = gate->delay("a");
    EXPECT_DOUBLE_EQ(retrieved->mean(), 10.0);
}

// Test: Exception when delay not set
TEST_F(GateTest, ExceptionWhenDelayNotSet) {
    Gate gate;
    gate->set_type_name("test_gate");

    EXPECT_THROW((void)gate->delay("a", "y"), Nh::RuntimeException);
}

// Test: Create instance
TEST_F(GateTest, CreateInstance) {
    Gate gate;
    gate->set_type_name("test_gate");

    Instance inst = gate.create_instance();
    EXPECT_TRUE(inst.get() != nullptr);
    EXPECT_FALSE(inst->name().empty());
    EXPECT_NE(inst->name().find("test_gate"), std::string::npos);
}

// Test: Create multiple instances
TEST_F(GateTest, CreateMultipleInstances) {
    Gate gate;
    gate->set_type_name("test_gate");

    Instance inst1 = gate.create_instance();
    Instance inst2 = gate.create_instance();
    Instance inst3 = gate.create_instance();

    EXPECT_NE(inst1->name(), inst2->name());
    EXPECT_NE(inst2->name(), inst3->name());
    EXPECT_NE(inst1->name(), inst3->name());
}

// Test: Instance set and get name
TEST_F(GateTest, InstanceSetAndGetName) {
    Gate gate;
    gate->set_type_name("test_gate");

    Instance inst = gate.create_instance();
    inst->set_name("inst1");
    EXPECT_EQ(inst->name(), "inst1");
}

// Test: Instance set input
TEST_F(GateTest, InstanceSetInput) {
    Gate gate;
    gate->set_type_name("test_gate");

    Normal delay(10.0, 2.0);
    gate->set_delay("a", "y", delay);

    Instance inst = gate.create_instance();

    Normal input_signal(5.0, 1.0);
    inst->set_input("a", input_signal);

    // Should not throw
    EXPECT_NO_THROW(inst->set_input("a", input_signal));
}

// Test: Instance set input with invalid pin name
TEST_F(GateTest, InstanceSetInputInvalidPin) {
    Gate gate;
    gate->set_type_name("test_gate");

    Normal delay(10.0, 2.0);
    gate->set_delay("a", "y", delay);

    Instance inst = gate.create_instance();

    Normal input_signal(5.0, 1.0);
    // Should throw when trying to set input for non-existent delay
    EXPECT_THROW(inst->set_input("b", input_signal), Nh::RuntimeException);
}

// Test: Instance output calculation
TEST_F(GateTest, InstanceOutputCalculation) {
    Gate gate;
    gate->set_type_name("test_gate");

    Normal delay(10.0, 2.0);
    gate->set_delay("a", "y", delay);

    Instance inst = gate.create_instance();

    Normal input_signal(5.0, 1.0);
    inst->set_input("a", input_signal);

    RandomVar output = inst->output("y");
    EXPECT_DOUBLE_EQ(output->mean(), 15.0);  // 5.0 + 10.0
}

// Test: Instance output with multiple inputs
TEST_F(GateTest, InstanceOutputMultipleInputs) {
    Gate gate;
    gate->set_type_name("test_gate");

    Normal delay_a(10.0, 2.0);
    Normal delay_b(15.0, 3.0);
    gate->set_delay("a", "y", delay_a);
    gate->set_delay("b", "y", delay_b);

    Instance inst = gate.create_instance();

    Normal input_a(5.0, 1.0);
    Normal input_b(8.0, 1.5);
    inst->set_input("a", input_a);
    inst->set_input("b", input_b);

    RandomVar output = inst->output("y");
    // Output should be MAX of (5.0+10.0) and (8.0+15.0) = MAX(15.0, 23.0) = 23.0
    EXPECT_GE(output->mean(), 15.0);
    EXPECT_LE(output->mean(), 25.0);  // Allow some variance
}

// Test: Instance output exception when no delay set
TEST_F(GateTest, InstanceOutputExceptionNoDelay) {
    Gate gate;
    gate->set_type_name("test_gate");

    Instance inst = gate.create_instance();

    EXPECT_THROW(inst->output("y"), Nh::RuntimeException);
}

// Test: Instance output caching
TEST_F(GateTest, InstanceOutputCaching) {
    Gate gate;
    gate->set_type_name("test_gate");

    Normal delay(10.0, 2.0);
    gate->set_delay("a", "y", delay);

    Instance inst = gate.create_instance();

    Normal input_signal(5.0, 1.0);
    inst->set_input("a", input_signal);

    RandomVar output1 = inst->output("y");
    RandomVar output2 = inst->output("y");

    // Should return the same cached value
    EXPECT_DOUBLE_EQ(output1->mean(), output2->mean());
}

// Test: Get delays map
TEST_F(GateTest, GetDelaysMap) {
    Gate gate;
    gate->set_type_name("test_gate");

    Normal delay1(10.0, 2.0);
    Normal delay2(15.0, 3.0);

    gate->set_delay("a", "y", delay1);
    gate->set_delay("b", "y", delay2);

    const _Gate_::Delays& delays = gate->delays();
    EXPECT_EQ(delays.size(), 2);
}

TEST_F(GateTest, GateCopySharesUnderlyingBody) {
    Gate gate;
    gate->set_type_name("source_gate");

    Normal delay(5.0, 1.0);
    gate->set_delay("a", "y", delay);

    Gate copied = gate;
    EXPECT_EQ(copied->type_name(), "source_gate");
    copied->set_type_name("copied_gate");

    EXPECT_EQ(gate->type_name(), "copied_gate");
    const Normal& retrieved = copied->delay("a", "y");
    EXPECT_DOUBLE_EQ(retrieved->mean(), 5.0);
}

TEST_F(GateTest, GateAssignmentKeepsDelayInformation) {
    Gate first;
    first->set_type_name("first");
    Normal first_delay(3.0, 0.5);
    first->set_delay("a", "y", first_delay);

    Gate second;
    second->set_type_name("second");
    Normal second_delay(7.0, 1.5);
    second->set_delay("b", "y", second_delay);

    second = first;
    EXPECT_EQ(second->type_name(), "first");
    EXPECT_NO_THROW((void)second->delay("a", "y"));
    EXPECT_THROW((void)second->delay("b", "y"), Nh::RuntimeException);
}

TEST_F(GateTest, InstanceRemainsValidAfterGateGoesOutOfScope) {
    Instance inst;
    {
        Gate gate;
        gate->set_type_name("temp_gate");
        Normal delay(4.0, 0.4);
        gate->set_delay("a", "y", delay);
        inst = gate.create_instance();
        Normal input_signal(10.0, 1.0);
        inst->set_input("a", input_signal);
    }
    EXPECT_NO_THROW({
        RandomVar output = inst->output("y");
        EXPECT_DOUBLE_EQ(output->mean(), 14.0);
    });
}

TEST_F(GateTest, InstancesShareGateStateAcrossCopies) {
    Gate gate;
    gate->set_type_name("shared_gate");
    Normal delay(6.0, 0.9);
    gate->set_delay("a", "y", delay);

    Gate alias = gate;
    alias->set_type_name("alias_gate");

    Instance inst1 = gate.create_instance();
    Instance inst2 = alias.create_instance();

    EXPECT_NE(inst1->name(), inst2->name());

    Normal input_signal(2.0, 0.25);
    inst1->set_input("a", input_signal);
    inst2->set_input("a", input_signal);

    EXPECT_DOUBLE_EQ(inst1->output("y")->mean(), inst2->output("y")->mean());
    EXPECT_EQ(gate->type_name(), "alias_gate");
}
