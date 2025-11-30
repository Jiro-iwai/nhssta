/**
 * @file test_sensitivity_clone.cpp
 * @brief Tests for sensitivity analysis with cloned delays
 *
 * This test verifies that gradients propagate correctly through
 * Gate -> Instance -> output pathway, where delays are cloned.
 *
 * Problem: In InstanceImpl::output(), delays are cloned via gate_delay.clone().
 * The Expression tree is built on these clones, but getSensitivityResults()
 * iterates over the original gates_, not the clones.
 *
 * Solution: Track cloned delays in InstanceImpl so they can be accessed
 * for gradient collection.
 */

#include <gtest/gtest.h>

#include <cmath>
#include <iostream>

#include <nhssta/gate.hpp>

#include "expression.hpp"
#include "normal.hpp"
#include "random_variable.hpp"

namespace {

class SensitivityCloneTest : public ::testing::Test {
   protected:
    void SetUp() override {
        zero_all_grad();
    }
};

/**
 * Test that a simple gate instance properly tracks cloned delays.
 *
 * Gate: inverter with delay D ~ N(1.0, 0.04) (σ = 0.2)
 * Instance: input signal A ~ N(10.0, 1.0)
 * Output: A + D (cloned delay)
 *
 * Without fix: Original D's mean_expr() has gradient 0 (not used in tree)
 * With fix: Cloned D's mean_expr() should have gradient 1
 */
TEST_F(SensitivityCloneTest, SingleGate_ClonedDelayGradient) {
    using Nh::Gate;
    using Nh::Instance;
    using ::RandomVariable::Normal;
    using ::RandomVariable::RandomVariable;

    // Create a gate with a single delay
    Gate inverter;
    inverter->set_type_name("INV");
    Normal original_delay(1.0, 0.04);  // μ=1.0, σ=0.2
    inverter->set_delay("a", "y", original_delay);

    // Create instance
    Instance inst = inverter.create_instance();
    EXPECT_EQ(inst->name(), "INV:0");

    // Create input signal
    Normal input_signal(10.0, 1.0);  // μ=10.0, σ=1.0
    inst->set_input("a", input_signal);

    // Compute output: output = input + delay (cloned)
    RandomVariable output = inst->output("y");

    // Verify output statistics
    // E[output] = E[input] + E[delay] = 10.0 + 1.0 = 11.0
    EXPECT_NEAR(output->mean(), 11.0, 1e-6);

    // Build Expression tree and compute gradients
    Expression mean_expr = output->mean_expr();
    EXPECT_NEAR(mean_expr->value(), 11.0, 1e-4);

    mean_expr->backward();

    // Input signal's gradient should be 1.0 (directly contributes to mean)
    EXPECT_DOUBLE_EQ(input_signal->mean_expr()->gradient(), 1.0);

    // Original delay's gradient should be 0.0 (not used in Expression tree)
    // This is the bug we're testing for!
    double original_grad = original_delay->mean_expr()->gradient();

    // The cloned delay's gradient should be 1.0
    // But we can't directly access it without tracking

    // For now, verify the bug exists:
    // Original delay gradient is 0, but it SHOULD be 1 if we could access the clone
    std::cout << "\n=== SingleGate_ClonedDelayGradient ===\n";
    std::cout << "Output mean: " << mean_expr->value() << "\n";
    std::cout << "Input signal gradient (∂mean/∂μ_input): " << input_signal->mean_expr()->gradient()
              << "\n";
    std::cout << "Original delay gradient (∂mean/∂μ_delay): " << original_grad << "\n";
    std::cout << "Expected delay gradient: 1.0 (if clone tracking works)\n";

    // This test will FAIL if clone tracking is not implemented:
    // We expect original_grad == 0 because clone is not tracked
    // After fix, we need a way to get the cloned delay's gradient
    EXPECT_EQ(original_grad, 0.0) << "BUG: Original delay gradient should be 0 (not in tree)";
}

/**
 * Test that tracks cloned delays and verifies gradient collection.
 *
 * After implementing clone tracking in InstanceImpl:
 * - InstanceImpl should have used_delays_ member
 * - getSensitivityResults() should iterate over used_delays_
 */
TEST_F(SensitivityCloneTest, SingleGate_UsedDelaysTracking) {
    using Nh::Gate;
    using Nh::Instance;
    using ::RandomVariable::Normal;
    using ::RandomVariable::RandomVariable;

    Gate inverter;
    inverter->set_type_name("INV");
    Normal original_delay(1.0, 0.04);
    inverter->set_delay("a", "y", original_delay);

    Instance inst = inverter.create_instance();

    Normal input_signal(10.0, 1.0);
    inst->set_input("a", input_signal);

    RandomVariable output = inst->output("y");

    // Build Expression tree and compute gradients
    Expression mean_expr = output->mean_expr();
    mean_expr->backward();

    // Verify used_delays() returns the cloned delays
    const auto& used_delays = inst->used_delays();
    ASSERT_EQ(used_delays.size(), 1u);

    // Get the cloned delay
    Nh::GateImpl::IO io("a", "y");
    auto it = used_delays.find(io);
    ASSERT_NE(it, used_delays.end());

    const Normal& cloned_delay = it->second;

    // The cloned delay's mean_expr()->gradient() should be 1.0
    // (since output = input + delay, ∂output/∂μ_delay = 1)
    double cloned_grad = cloned_delay->mean_expr()->gradient();

    std::cout << "\n=== SingleGate_UsedDelaysTracking ===\n";
    std::cout << "used_delays().size(): " << used_delays.size() << "\n";
    std::cout << "Original delay gradient: " << original_delay->mean_expr()->gradient() << "\n";
    std::cout << "Cloned delay gradient: " << cloned_grad << "\n";
    std::cout << "Expected: 1.0\n";

    EXPECT_DOUBLE_EQ(cloned_grad, 1.0);
    EXPECT_NEAR(output->mean(), 11.0, 1e-6);
}

/**
 * Test multiple instances of the same gate.
 *
 * Each instance should have its own cloned delays,
 * and gradients should accumulate correctly.
 */
TEST_F(SensitivityCloneTest, MultipleInstances_SameGate) {
    using Nh::Gate;
    using Nh::Instance;
    using ::RandomVariable::Normal;
    using ::RandomVariable::RandomVariable;
    using ::RandomVariable::MAX;

    // Gate with one delay
    Gate inv;
    inv->set_type_name("INV");
    Normal delay(1.0, 0.04);
    inv->set_delay("a", "y", delay);

    // Create two instances
    Instance inst1 = inv.create_instance();
    Instance inst2 = inv.create_instance();

    // Different input signals
    Normal input1(10.0, 1.0);
    Normal input2(20.0, 1.0);

    inst1->set_input("a", input1);
    inst2->set_input("a", input2);

    RandomVariable out1 = inst1->output("y");
    RandomVariable out2 = inst2->output("y");

    // Verify outputs
    EXPECT_NEAR(out1->mean(), 11.0, 1e-6);  // 10 + 1
    EXPECT_NEAR(out2->mean(), 21.0, 1e-6);  // 20 + 1

    // MAX of outputs
    auto max_out = MAX(out1, out2);

    // Build Expression and compute gradients
    Expression mean_expr = max_out->mean_expr();
    mean_expr->backward();

    // Input2 dominates (larger mean), so its gradient should be larger
    double grad_input1 = input1->mean_expr()->gradient();
    double grad_input2 = input2->mean_expr()->gradient();

    std::cout << "\n=== MultipleInstances_SameGate ===\n";
    std::cout << "MAX mean: " << mean_expr->value() << "\n";
    std::cout << "Input1 gradient: " << grad_input1 << "\n";
    std::cout << "Input2 gradient: " << grad_input2 << "\n";
    std::cout << "Original delay gradient: " << delay->mean_expr()->gradient() << "\n";

    // out2 dominates, so grad_input2 > grad_input1
    EXPECT_GT(grad_input2, grad_input1);

    // Original delay gradient is 0 because clones are used
    EXPECT_EQ(delay->mean_expr()->gradient(), 0.0);
}

/**
 * Test that const delays (σ=0) are handled correctly.
 *
 * For σ=0, the Expression math functions may have numerical issues
 * (division by zero in -μ/σ).
 *
 * Solution: Skip const delays in sensitivity analysis (Issue #184 for future fix)
 */
TEST_F(SensitivityCloneTest, ConstDelay_SkipInSensitivity) {
    using Nh::Gate;
    using Nh::Instance;
    using ::RandomVariable::Normal;
    using ::RandomVariable::RandomVariable;

    // Gate with const delay (σ² = 0)
    Gate buf;
    buf->set_type_name("BUF");
    Normal const_delay(1.0, 0.0);  // μ=1.0, σ²=0 (const)
    buf->set_delay("a", "y", const_delay);

    Instance inst = buf.create_instance();

    Normal input(10.0, 1.0);
    inst->set_input("a", input);

    RandomVariable output = inst->output("y");

    // Output mean should still be correct
    EXPECT_NEAR(output->mean(), 11.0, 1e-6);

    // Building Expression tree for const delay should work
    // (but gradients for const delay should be skipped)
    Expression mean_expr = output->mean_expr();
    
    // This should not throw even with σ=0
    EXPECT_NO_THROW({
        mean_expr->backward();
    });

    std::cout << "\n=== ConstDelay_SkipInSensitivity ===\n";
    std::cout << "Output mean: " << mean_expr->value() << "\n";
    std::cout << "Input gradient: " << input->mean_expr()->gradient() << "\n";
    std::cout << "Const delay should be skipped in sensitivity output (Issue #184)\n";

    // Input gradient should still be 1.0
    EXPECT_DOUBLE_EQ(input->mean_expr()->gradient(), 1.0);
}

}  // namespace

