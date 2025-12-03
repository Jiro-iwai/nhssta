// -*- c++ -*-
// Test: Simple circuit using RandomVariable
// Purpose: Test basic RandomVariable operations in a simple circuit

#include "../src/normal.hpp"
#include "../src/add.hpp"
#include "../src/sub.hpp"
#include "../src/max.hpp"
#include "../src/random_variable.hpp"
#include "../src/expression.hpp"

#include <gtest/gtest.h>
#include <iostream>
#include <iomanip>
#include <cmath>

using RandomVar = RandomVariable::RandomVariable;
using Normal = RandomVariable::Normal;
using RandomVariable::MAX;
using RandomVariable::MAX0;

class SimpleCircuitTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Clear expression table
        ExpressionImpl::zero_all_grad();
    }

    void TearDown() override {
        ExpressionImpl::zero_all_grad();
    }
    
    // Helper function to zero all gradients
    void zero_all_grad() {
        ExpressionImpl::zero_all_grad();
    }
};

// Test: Simple circuit with two inputs, addition, and MAX operation
// Circuit: 
//   Input1 (G0) ──┐
//                  ├──► [ADD] ──► G1 ──┐
//   Input2 (G2) ──┘                     │
//                                        ├──► [MAX] ──► Output (G3)
//   Input3 (G4) ────────────────────────┘
TEST_F(SimpleCircuitTest, BasicCircuitOperations) {
    std::cout << "\n=== Testing simple circuit operations ===" << std::endl;
    std::cout << std::setprecision(6);

    // Create input signals (normal distributions)
    RandomVar G0 = Normal(10.0, 1.0);  // Input1: mean=10, variance=1
    RandomVar G2 = Normal(15.0, 2.0);  // Input2: mean=15, variance=2
    RandomVar G4 = Normal(20.0, 1.5);  // Input3: mean=20, variance=1.5

    G0->set_name("G0");
    G2->set_name("G2");
    G4->set_name("G4");

    std::cout << "\nInput signals:" << std::endl;
    std::cout << "  G0: mean=" << G0->mean() << ", variance=" << G0->variance() 
              << ", std=" << G0->standard_deviation() << std::endl;
    std::cout << "  G2: mean=" << G2->mean() << ", variance=" << G2->variance() 
              << ", std=" << G2->standard_deviation() << std::endl;
    std::cout << "  G4: mean=" << G4->mean() << ", variance=" << G4->variance() 
              << ", std=" << G4->standard_deviation() << std::endl;

    // Circuit operation: G1 = G0 + G2 (addition)
    RandomVar G1 = G0 + G2;
    G1->set_name("G1");

    std::cout << "\nAfter addition (G1 = G0 + G2):" << std::endl;
    std::cout << "  G1: mean=" << G1->mean() << ", variance=" << G1->variance() 
              << ", std=" << G1->standard_deviation() << std::endl;

    // Expected: E[G1] = E[G0] + E[G2] = 10 + 15 = 25
    EXPECT_NEAR(G1->mean(), 25.0, 1e-10);
    
    // Expected: Var[G1] = Var[G0] + Var[G2] + 2*Cov(G0, G2)
    // For independent inputs: Cov(G0, G2) = 0
    // Var[G1] = 1.0 + 2.0 = 3.0
    EXPECT_NEAR(G1->variance(), 3.0, 1e-10);

    // Circuit operation: G3 = MAX(G1, G4) (maximum)
    RandomVar G3 = MAX(G1, G4);
    G3->set_name("G3");

    std::cout << "\nAfter MAX operation (G3 = MAX(G1, G4)):" << std::endl;
    std::cout << "  G3: mean=" << G3->mean() << ", variance=" << G3->variance() 
              << ", std=" << G3->standard_deviation() << std::endl;

    // Expected: E[G3] should be >= max(E[G1], E[G4]) = max(25, 20) = 25
    // But MAX operation increases mean due to correlation
    EXPECT_GE(G3->mean(), 25.0);
    
    // G3 should have non-zero variance
    EXPECT_GT(G3->variance(), 0.0);

    std::cout << "\nCircuit summary:" << std::endl;
    std::cout << "  Input1 (G0): " << G0->mean() << " ± " << G0->standard_deviation() << std::endl;
    std::cout << "  Input2 (G2): " << G2->mean() << " ± " << G2->standard_deviation() << std::endl;
    std::cout << "  Input3 (G4): " << G4->mean() << " ± " << G4->standard_deviation() << std::endl;
    std::cout << "  Sum (G1):    " << G1->mean() << " ± " << G1->standard_deviation() << std::endl;
    std::cout << "  Output (G3): " << G3->mean() << " ± " << G3->standard_deviation() << std::endl;
}

// Test: Circuit with subtraction
// Circuit:
//   Input1 (G0) ──┐
//                  ├──► [SUB] ──► G1 ──► Output
//   Input2 (G2) ──┘
TEST_F(SimpleCircuitTest, SubtractionCircuit) {
    std::cout << "\n=== Testing subtraction circuit ===" << std::endl;
    std::cout << std::setprecision(6);

    RandomVar G0 = Normal(20.0, 2.0);
    RandomVar G2 = Normal(10.0, 1.0);

    G0->set_name("G0");
    G2->set_name("G2");

    // G1 = G0 - G2
    RandomVar G1 = G0 - G2;
    G1->set_name("G1");

    std::cout << "\nSubtraction (G1 = G0 - G2):" << std::endl;
    std::cout << "  G0: mean=" << G0->mean() << ", variance=" << G0->variance() << std::endl;
    std::cout << "  G2: mean=" << G2->mean() << ", variance=" << G2->variance() << std::endl;
    std::cout << "  G1: mean=" << G1->mean() << ", variance=" << G1->variance() << std::endl;

    // Expected: E[G1] = E[G0] - E[G2] = 20 - 10 = 10
    EXPECT_NEAR(G1->mean(), 10.0, 1e-10);
    
    // Expected: Var[G1] = Var[G0] + Var[G2] - 2*Cov(G0, G2)
    // For independent inputs: Cov(G0, G2) = 0
    // Var[G1] = 2.0 + 1.0 = 3.0
    EXPECT_NEAR(G1->variance(), 3.0, 1e-10);
}

// Test: Circuit with MAX0 operation
// Circuit:
//   Input (G0) ──► [MAX0] ──► G1 ──► Output
TEST_F(SimpleCircuitTest, Max0Circuit) {
    std::cout << "\n=== Testing MAX0 circuit ===" << std::endl;
    std::cout << std::setprecision(6);

    // Test case 1: Positive mean
    {
        RandomVar G0 = Normal(10.0, 1.0);
        G0->set_name("G0");

        RandomVar G1 = MAX0(G0);
        G1->set_name("G1");

        std::cout << "\nMAX0 with positive mean (G1 = MAX0(G0)):" << std::endl;
        std::cout << "  G0: mean=" << G0->mean() << ", variance=" << G0->variance() << std::endl;
        std::cout << "  G1: mean=" << G1->mean() << ", variance=" << G1->variance() << std::endl;

        // For positive mean with large value, MAX0 should be very close to the original
        // When mean >> std, MAX0 ≈ original (with very small reduction)
        EXPECT_GT(G1->mean(), 0.0);
        EXPECT_LE(G1->mean(), G0->mean());  // MAX0 reduces mean slightly (or equal for large mean)
    }

    // Test case 2: Negative mean
    {
        RandomVar G0 = Normal(-5.0, 1.0);
        G0->set_name("G0");

        RandomVar G1 = MAX0(G0);
        G1->set_name("G1");

        std::cout << "\nMAX0 with negative mean (G1 = MAX0(G0)):" << std::endl;
        std::cout << "  G0: mean=" << G0->mean() << ", variance=" << G0->variance() << std::endl;
        std::cout << "  G1: mean=" << G1->mean() << ", variance=" << G1->variance() << std::endl;

        // For negative mean, MAX0 should be close to 0
        EXPECT_GE(G1->mean(), 0.0);
        EXPECT_LT(G1->mean(), 1.0);  // Should be small positive value
    }
}

// Test: Complex circuit with multiple operations
// Circuit:
//   Input1 (G0) ──┐
//                  ├──► [ADD] ──► G1 ──┐
//   Input2 (G2) ──┘                     │
//                                        ├──► [MAX] ──► G3 ──┐
//   Input3 (G4) ────────────────────────┘                     │
//                                                              ├──► [ADD] ──► Output (G5)
//   Input4 (G6) ─────────────────────────────────────────────┘
TEST_F(SimpleCircuitTest, ComplexCircuit) {
    std::cout << "\n=== Testing complex circuit ===" << std::endl;
    std::cout << std::setprecision(6);

    RandomVar G0 = Normal(5.0, 0.5);
    RandomVar G2 = Normal(8.0, 1.0);
    RandomVar G4 = Normal(12.0, 1.5);
    RandomVar G6 = Normal(3.0, 0.8);

    G0->set_name("G0");
    G2->set_name("G2");
    G4->set_name("G4");
    G6->set_name("G6");

    // G1 = G0 + G2
    RandomVar G1 = G0 + G2;
    G1->set_name("G1");

    // G3 = MAX(G1, G4)
    RandomVar G3 = MAX(G1, G4);
    G3->set_name("G3");

    // G5 = G3 + G6
    RandomVar G5 = G3 + G6;
    G5->set_name("G5");

    std::cout << "\nComplex circuit results:" << std::endl;
    std::cout << "  G0: " << G0->mean() << " ± " << G0->standard_deviation() << std::endl;
    std::cout << "  G2: " << G2->mean() << " ± " << G2->standard_deviation() << std::endl;
    std::cout << "  G4: " << G4->mean() << " ± " << G4->standard_deviation() << std::endl;
    std::cout << "  G6: " << G6->mean() << " ± " << G6->standard_deviation() << std::endl;
    std::cout << "  G1 = G0 + G2: " << G1->mean() << " ± " << G1->standard_deviation() << std::endl;
    std::cout << "  G3 = MAX(G1, G4): " << G3->mean() << " ± " << G3->standard_deviation() << std::endl;
    std::cout << "  G5 = G3 + G6: " << G5->mean() << " ± " << G5->standard_deviation() << std::endl;

    // Verify intermediate results
    EXPECT_NEAR(G1->mean(), 13.0, 1e-10);  // 5 + 8 = 13
    EXPECT_GE(G3->mean(), 13.0);  // MAX(13, 12) >= 13
    EXPECT_GT(G5->mean(), 16.0);  // G3 + 3 > 13 + 3 = 16
}

// Test: Verify statistical properties
TEST_F(SimpleCircuitTest, StatisticalProperties) {
    std::cout << "\n=== Testing statistical properties ===" << std::endl;
    std::cout << std::setprecision(6);

    RandomVar G0 = Normal(10.0, 4.0);  // mean=10, variance=4, std=2
    G0->set_name("G0");

    std::cout << "\nStatistical properties of G0:" << std::endl;
    std::cout << "  Mean: " << G0->mean() << std::endl;
    std::cout << "  Variance: " << G0->variance() << std::endl;
    std::cout << "  Standard deviation: " << G0->standard_deviation() << std::endl;
    std::cout << "  Coefficient of variation: " << G0->coefficient_of_variation() << std::endl;
    std::cout << "  Relative error: " << G0->relative_error() << std::endl;

    EXPECT_NEAR(G0->mean(), 10.0, 1e-10);
    EXPECT_NEAR(G0->variance(), 4.0, 1e-10);
    EXPECT_NEAR(G0->standard_deviation(), 2.0, 1e-10);
    EXPECT_NEAR(G0->coefficient_of_variation(), 0.2, 1e-10);  // std/mean = 2/10 = 0.2
    EXPECT_NEAR(G0->relative_error(), 0.2, 1e-10);  // Same as coefficient_of_variation
}

// ============================================================================
// Sensitivity Analysis Tests
// ============================================================================

// Test: Sensitivity analysis for simple addition circuit
// Circuit: G1 = G0 + G2
// Sensitivities: ∂E[G1]/∂μ_G0, ∂E[G1]/∂μ_G2, etc.
//
// Expected results:
// - For addition: E[G1] = E[G0] + E[G2]
// - ∂E[G1]/∂μ_G0 = 1 (G0's mean directly contributes to G1's mean)
// - ∂E[G1]/∂μ_G2 = 1 (G2's mean directly contributes to G1's mean)
// - ∂E[G1]/∂σ_G0 = 0 (mean doesn't depend on standard deviation)
// - ∂E[G1]/∂σ_G2 = 0 (mean doesn't depend on standard deviation)
TEST_F(SimpleCircuitTest, Sensitivity_AdditionCircuit) {
    std::cout << "\n=== Testing sensitivity analysis for addition circuit ===" << std::endl;
    std::cout << std::setprecision(6);

    RandomVar G0 = Normal(10.0, 1.0);
    RandomVar G2 = Normal(15.0, 2.0);

    G0->set_name("G0");
    G2->set_name("G2");

    // G1 = G0 + G2
    RandomVar G1 = G0 + G2;
    G1->set_name("G1");

    // Compute mean expression and its sensitivity
    Expression mean_G1 = G1->mean_expr();
    double expected_mean = G1->mean();
    EXPECT_NEAR(mean_G1->value(), expected_mean, 1e-10);

    // Compute gradients
    zero_all_grad();
    mean_G1->backward();

    double grad_mu_G0 = G0->mean_expr()->gradient();
    double grad_mu_G2 = G2->mean_expr()->gradient();
    double grad_sigma_G0 = G0->std_expr()->gradient();
    double grad_sigma_G2 = G2->std_expr()->gradient();

    std::cout << "\nSensitivity analysis for G1 = G0 + G2:" << std::endl;
    std::cout << "  E[G1] = " << mean_G1->value() << std::endl;
    std::cout << "  ∂E[G1]/∂μ_G0 = " << grad_mu_G0 << std::endl;
    std::cout << "  ∂E[G1]/∂μ_G2 = " << grad_mu_G2 << std::endl;
    std::cout << "  ∂E[G1]/∂σ_G0 = " << grad_sigma_G0 << std::endl;
    std::cout << "  ∂E[G1]/∂σ_G2 = " << grad_sigma_G2 << std::endl;

    // For addition: ∂E[G1]/∂μ_G0 = 1, ∂E[G1]/∂μ_G2 = 1
    EXPECT_DOUBLE_EQ(grad_mu_G0, 1.0);
    EXPECT_DOUBLE_EQ(grad_mu_G2, 1.0);
    
    // Mean doesn't depend on standard deviation
    EXPECT_DOUBLE_EQ(grad_sigma_G0, 0.0);
    EXPECT_DOUBLE_EQ(grad_sigma_G2, 0.0);
}

// Test: Sensitivity analysis for subtraction circuit
// Circuit: G1 = G0 - G2
//
// Expected results:
// - For subtraction: E[G1] = E[G0] - E[G2]
// - ∂E[G1]/∂μ_G0 = 1 (G0's mean directly contributes positively)
// - ∂E[G1]/∂μ_G2 = -1 (G2's mean contributes negatively)
TEST_F(SimpleCircuitTest, Sensitivity_SubtractionCircuit) {
    std::cout << "\n=== Testing sensitivity analysis for subtraction circuit ===" << std::endl;
    std::cout << std::setprecision(6);

    RandomVar G0 = Normal(20.0, 2.0);
    RandomVar G2 = Normal(10.0, 1.0);

    G0->set_name("G0");
    G2->set_name("G2");

    // G1 = G0 - G2
    RandomVar G1 = G0 - G2;
    G1->set_name("G1");

    Expression mean_G1 = G1->mean_expr();
    zero_all_grad();
    mean_G1->backward();

    double grad_mu_G0 = G0->mean_expr()->gradient();
    double grad_mu_G2 = G2->mean_expr()->gradient();

    std::cout << "\nSensitivity analysis for G1 = G0 - G2:" << std::endl;
    std::cout << "  E[G1] = " << mean_G1->value() << std::endl;
    std::cout << "  ∂E[G1]/∂μ_G0 = " << grad_mu_G0 << std::endl;
    std::cout << "  ∂E[G1]/∂μ_G2 = " << grad_mu_G2 << std::endl;

    // For subtraction: ∂E[G1]/∂μ_G0 = 1, ∂E[G1]/∂μ_G2 = -1
    EXPECT_DOUBLE_EQ(grad_mu_G0, 1.0);
    EXPECT_DOUBLE_EQ(grad_mu_G2, -1.0);
}

// Test: Sensitivity analysis for MAX circuit
// Circuit: G3 = MAX(G1, G4)
//
// Expected results:
// - For MAX operation: MAX(G1, G4) = G1 + MAX0(G4 - G1) (Clark approximation)
// - Both inputs contribute to the output, with the larger mean dominating
// - ∂E[G3]/∂μ_G1 + ∂E[G3]/∂μ_G4 ≈ 1.0 (both inputs contribute, sum to 1.0)
// - If E[G1] > E[G4]: ∂E[G3]/∂μ_G1 > ∂E[G3]/∂μ_G4 (G1 dominates)
// - If E[G1] < E[G4]: ∂E[G3]/∂μ_G1 < ∂E[G3]/∂μ_G4 (G4 dominates)
// - Both gradients are between 0 and 1 (partial contribution)
// - σ gradients can be any value (not necessarily sum to 1)
TEST_F(SimpleCircuitTest, Sensitivity_MaxCircuit) {
    std::cout << "\n=== Testing sensitivity analysis for MAX circuit ===" << std::endl;
    std::cout << std::setprecision(6);

    RandomVar G1 = Normal(25.0, 3.0);
    RandomVar G4 = Normal(20.0, 1.5);

    G1->set_name("G1");
    G4->set_name("G4");

    // G3 = MAX(G1, G4)
    RandomVar G3 = MAX(G1, G4);
    G3->set_name("G3");

    Expression mean_G3 = G3->mean_expr();
    double expected_mean = G3->mean();
    EXPECT_NEAR(mean_G3->value(), expected_mean, 1e-2);  // MAX has approximation errors

    zero_all_grad();
    mean_G3->backward();

    double grad_mu_G1 = G1->mean_expr()->gradient();
    double grad_mu_G4 = G4->mean_expr()->gradient();
    double grad_sigma_G1 = G1->std_expr()->gradient();
    double grad_sigma_G4 = G4->std_expr()->gradient();

    std::cout << "\nSensitivity analysis for G3 = MAX(G1, G4):" << std::endl;
    std::cout << "  E[G3] = " << mean_G3->value() << std::endl;
    std::cout << "  ∂E[G3]/∂μ_G1 = " << grad_mu_G1 << std::endl;
    std::cout << "  ∂E[G3]/∂μ_G4 = " << grad_mu_G4 << std::endl;
    std::cout << "  ∂E[G3]/∂σ_G1 = " << grad_sigma_G1 << std::endl;
    std::cout << "  ∂E[G3]/∂σ_G4 = " << grad_sigma_G4 << std::endl;

    // Validation:
    // 1. Since E[G1] = 25 > E[G4] = 20, G1 dominates in MAX(G1, G4)
    //    Therefore, ∂E[G3]/∂μ_G1 should be larger than ∂E[G3]/∂μ_G4
    EXPECT_GT(grad_mu_G1, grad_mu_G4);
    
    // 2. Both gradients should be between 0 and 1 (partial contribution)
    //    This reflects that both inputs contribute to the MAX output
    EXPECT_GE(grad_mu_G1, 0.0);
    EXPECT_LE(grad_mu_G1, 1.0);
    EXPECT_GE(grad_mu_G4, 0.0);
    EXPECT_LE(grad_mu_G4, 1.0);
    
    // 3. Sum of gradients should be approximately 1.0
    //    This is a key property of MAX operation: both inputs contribute,
    //    and their contributions sum to 1.0 (total sensitivity)
    EXPECT_NEAR(grad_mu_G1 + grad_mu_G4, 1.0, 0.1);
}

// Test: Sensitivity analysis for complex circuit
// Circuit: G1 = G0 + G2, G3 = MAX(G1, G4), G5 = G3 + G6
//
// Expected results:
// - G1 = G0 + G2: E[G1] = E[G0] + E[G2] = 5 + 8 = 13
// - G3 = MAX(G1, G4): Since E[G1] = 13 > E[G4] = 12, G1 dominates but G4 also contributes
// - G5 = G3 + G6: E[G5] = E[G3] + E[G6]
//
// Gradient propagation through the circuit:
// - ∂E[G5]/∂μ_G6 = 1.0 (direct addition: G6 contributes directly)
// - ∂E[G5]/∂μ_G0 = ∂E[G5]/∂E[G3] × ∂E[G3]/∂E[G1] × ∂E[G1]/∂μ_G0
//                 = 1.0 × g1 × 1.0 = g1
//   where g1 = ∂E[G3]/∂E[G1] (gradient of MAX w.r.t. G1)
// - ∂E[G5]/∂μ_G2 = g1 (same as G0, since G1 = G0 + G2)
// - ∂E[G5]/∂μ_G4 = g4, where g4 = ∂E[G3]/∂E[G4] (gradient of MAX w.r.t. G4)
// - Note: g1 + g4 ≈ 1.0 (MAX operation property)
//
// Total sum of gradients:
// - ∂E[G5]/∂μ_G0 + ∂E[G5]/∂μ_G2 + ∂E[G5]/∂μ_G4 + ∂E[G5]/∂μ_G6
//   = g1 + g1 + g4 + 1.0
//   = 2g1 + g4 + 1.0
//   = g1 + (g1 + g4) + 1.0
//   ≈ g1 + 1.0 + 1.0
//   ≈ g1 + 2.0
//   Since g1 ≈ 0.7, total ≈ 2.7 (each operation contributes independently)
TEST_F(SimpleCircuitTest, Sensitivity_ComplexCircuit) {
    std::cout << "\n=== Testing sensitivity analysis for complex circuit ===" << std::endl;
    std::cout << std::setprecision(6);

    RandomVar G0 = Normal(5.0, 0.5);
    RandomVar G2 = Normal(8.0, 1.0);
    RandomVar G4 = Normal(12.0, 1.5);
    RandomVar G6 = Normal(3.0, 0.8);

    G0->set_name("G0");
    G2->set_name("G2");
    G4->set_name("G4");
    G6->set_name("G6");

    // Build circuit
    RandomVar G1 = G0 + G2;
    RandomVar G3 = MAX(G1, G4);
    RandomVar G5 = G3 + G6;

    G1->set_name("G1");
    G3->set_name("G3");
    G5->set_name("G5");

    // Analyze sensitivity of final output G5
    Expression mean_G5 = G5->mean_expr();
    double expected_mean = G5->mean();
    EXPECT_NEAR(mean_G5->value(), expected_mean, 1e-2);

    zero_all_grad();
    mean_G5->backward();

    double grad_mu_G0 = G0->mean_expr()->gradient();
    double grad_mu_G2 = G2->mean_expr()->gradient();
    double grad_mu_G4 = G4->mean_expr()->gradient();
    double grad_mu_G6 = G6->mean_expr()->gradient();

    std::cout << "\nSensitivity analysis for complex circuit:" << std::endl;
    std::cout << "  Circuit: G1 = G0 + G2, G3 = MAX(G1, G4), G5 = G3 + G6" << std::endl;
    std::cout << "  E[G5] = " << mean_G5->value() << std::endl;
    std::cout << "\nSensitivities:" << std::endl;
    std::cout << "  ∂E[G5]/∂μ_G0 = " << grad_mu_G0 << std::endl;
    std::cout << "  ∂E[G5]/∂μ_G2 = " << grad_mu_G2 << std::endl;
    std::cout << "  ∂E[G5]/∂μ_G4 = " << grad_mu_G4 << std::endl;
    std::cout << "  ∂E[G5]/∂μ_G6 = " << grad_mu_G6 << std::endl;

    // Validation:
    // 1. G6 is directly added to G3, so ∂E[G5]/∂μ_G6 = 1.0
    //    This is independent of the MAX operation
    EXPECT_DOUBLE_EQ(grad_mu_G6, 1.0);
    
    // 2. G0 and G2 contribute equally through G1
    //    Since G1 = G0 + G2, both have the same gradient w.r.t. G1's mean
    //    Therefore, ∂E[G5]/∂μ_G0 = ∂E[G5]/∂μ_G2 (chain rule through G1)
    EXPECT_NEAR(grad_mu_G0, grad_mu_G2, 1e-10);
    
    // 3. All gradients should be non-negative
    //    Increasing any input's mean increases the output's mean
    EXPECT_GE(grad_mu_G0, 0.0);
    EXPECT_GE(grad_mu_G2, 0.0);
    EXPECT_GE(grad_mu_G4, 0.0);
    EXPECT_GE(grad_mu_G6, 0.0);
    
    // 4. Verify MAX operation contribution
    //    The contribution from MAX(G1, G4) through G1 and G4:
    //    - G1 = G0 + G2, so G0 and G2 each contribute through G1
    //    - Let g1 = ∂E[G3]/∂E[G1] and g4 = ∂E[G3]/∂E[G4]
    //    - Then: ∂E[G5]/∂μ_G0 = g1 × 1.0, ∂E[G5]/∂μ_G2 = g1 × 1.0, ∂E[G5]/∂μ_G4 = g4
    //    - MAX contribution = g1 + g1 + g4 = 2g1 + g4
    //    - Since g1 + g4 ≈ 1.0, we have: 2g1 + g4 = g1 + (g1 + g4) ≈ g1 + 1.0
    //    - With g1 ≈ 0.718, MAX contribution ≈ 1.718
    double max_contribution = grad_mu_G0 + grad_mu_G2 + grad_mu_G4;
    EXPECT_GT(max_contribution, 1.0);  // MAX contribution > 1.0 because G0 and G2 both contribute
    EXPECT_LT(max_contribution, 2.0);   // MAX contribution < 2.0 because g1 + g4 ≈ 1.0
    
    // 5. Total sum should be MAX contribution + G6 contribution
    //    ≈ 1.718 + 1.0 = 2.718 (each operation contributes independently)
    double total_sum = grad_mu_G0 + grad_mu_G2 + grad_mu_G4 + grad_mu_G6;
    EXPECT_GT(total_sum, 2.0);   // Total > 2.0 because MAX contribution > 1.0
    EXPECT_LT(total_sum, 3.0);   // Total < 3.0 because MAX contribution < 2.0
}

// Test: Sensitivity analysis for variance
// Circuit: G1 = G0 + G2
// Analyze: ∂Var[G1]/∂σ_G0, ∂Var[G1]/∂σ_G2
//
// Expected results:
// - For independent addition: Var[G1] = Var[G0] + Var[G2] = σ_G0² + σ_G2²
// - ∂Var[G1]/∂σ_G0 = ∂(σ_G0²)/∂σ_G0 = 2σ_G0
// - ∂Var[G1]/∂σ_G2 = ∂(σ_G2²)/∂σ_G2 = 2σ_G2
// - For G0: σ_G0 = 2, so ∂Var[G1]/∂σ_G0 = 2 × 2 = 4
// - For G2: σ_G2 = 1, so ∂Var[G1]/∂σ_G2 = 2 × 1 = 2
TEST_F(SimpleCircuitTest, Sensitivity_Variance) {
    std::cout << "\n=== Testing sensitivity analysis for variance ===" << std::endl;
    std::cout << std::setprecision(6);

    RandomVar G0 = Normal(10.0, 4.0);  // σ = 2
    RandomVar G2 = Normal(5.0, 1.0);    // σ = 1

    G0->set_name("G0");
    G2->set_name("G2");

    // G1 = G0 + G2
    RandomVar G1 = G0 + G2;
    G1->set_name("G1");

    // Analyze variance sensitivity
    Expression var_G1 = G1->var_expr();
    double expected_var = G1->variance();
    EXPECT_NEAR(var_G1->value(), expected_var, 1e-10);

    zero_all_grad();
    var_G1->backward();

    double grad_sigma_G0 = G0->std_expr()->gradient();
    double grad_sigma_G2 = G2->std_expr()->gradient();

    std::cout << "\nVariance sensitivity analysis for G1 = G0 + G2:" << std::endl;
    std::cout << "  Var[G1] = " << var_G1->value() << std::endl;
    std::cout << "  ∂Var[G1]/∂σ_G0 = " << grad_sigma_G0 << std::endl;
    std::cout << "  ∂Var[G1]/∂σ_G2 = " << grad_sigma_G2 << std::endl;

    // Validation:
    // For independent addition: Var[G1] = Var[G0] + Var[G2] = σ_G0² + σ_G2²
    // - ∂Var[G1]/∂σ_G0 = ∂(σ_G0²)/∂σ_G0 = 2σ_G0 = 2 × 2 = 4
    // - ∂Var[G1]/∂σ_G2 = ∂(σ_G2²)/∂σ_G2 = 2σ_G2 = 2 × 1 = 2
    // These values reflect how variance changes with standard deviation
    EXPECT_NEAR(grad_sigma_G0, 4.0, 1e-10);  // 2 * σ_G0 = 2 * 2 = 4
    EXPECT_NEAR(grad_sigma_G2, 2.0, 1e-10);  // 2 * σ_G2 = 2 * 1 = 2
}

