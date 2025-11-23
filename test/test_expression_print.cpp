#include <gtest/gtest.h>
#include <sstream>
#include <iomanip>
#include <functional>
#include "expression.hpp"

// Test fixture for Expression print tests
class ExpressionPrintTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Each test creates fresh expressions
    }

    void TearDown() override {
        // Cleanup after each test
    }

    // Helper function to capture std::cout output
    std::string captureOutput(std::function<void()> func) {
        std::ostringstream oss;
        std::streambuf* oldCout = std::cout.rdbuf();
        std::cout.rdbuf(oss.rdbuf());
        
        func();
        
        std::cout.rdbuf(oldCout);
        return oss.str();
    }
};

// Test print_all function with Const expression
TEST_F(ExpressionPrintTest, PrintAllWithConst) {
    Const c(3.14);
    
    std::string output = captureOutput([]() {
        print_all();
    });

    // Expected format: id (5 chars), value (10 chars), op (10 chars), left (10 chars), right (10 chars), left_id (10 chars), right_id (10 chars)
    EXPECT_FALSE(output.empty());
    // Check that output contains the value and CONST
    EXPECT_NE(output.find("3.14"), std::string::npos);
    EXPECT_NE(output.find("CONST"), std::string::npos);
}

// Test print_all function with Variable expression
TEST_F(ExpressionPrintTest, PrintAllWithVariable) {
    Variable x;
    x = 2.5;
    
    std::string output = captureOutput([]() {
        print_all();
    });

    EXPECT_FALSE(output.empty());
    EXPECT_NE(output.find("2.5"), std::string::npos);
    EXPECT_NE(output.find("PARAM"), std::string::npos);
}

// Test print_all function with addition expression
TEST_F(ExpressionPrintTest, PrintAllWithAddition) {
    Variable x, y;
    Expression z = x + y;
    
    x = 1.0;
    y = 2.0;
    
    std::string output = captureOutput([]() {
        print_all();
    });

    EXPECT_FALSE(output.empty());
    EXPECT_NE(output.find("+"), std::string::npos);
}

// Test print_all function with multiplication expression
TEST_F(ExpressionPrintTest, PrintAllWithMultiplication) {
    Variable x, y;
    Expression z = x * y;
    
    std::string output = captureOutput([]() {
        print_all();
    });

    EXPECT_FALSE(output.empty());
    EXPECT_NE(output.find("x"), std::string::npos);
}

// Test print_all function with multiple expressions
TEST_F(ExpressionPrintTest, PrintAllWithMultipleExpressions) {
    Variable x, y;
    Expression z1 = x + y;
    Expression z2 = x * y;
    
    std::string output = captureOutput([]() {
        print_all();
    });

    EXPECT_FALSE(output.empty());
    // Should contain multiple expressions
    EXPECT_NE(output.find("PARAM"), std::string::npos);
    EXPECT_NE(output.find("+"), std::string::npos);
    EXPECT_NE(output.find("x"), std::string::npos);
}

// Test print format consistency - verify column widths
TEST_F(ExpressionPrintTest, PrintFormatConsistency) {
    Const c(123.456789);
    
    std::string output = captureOutput([]() {
        print_all();
    });

    // Verify output has consistent structure
    EXPECT_FALSE(output.empty());
    
    // Check that the output contains newlines (each expression on a line)
    EXPECT_NE(output.find('\n'), std::string::npos);
    
    // Verify format: should have consistent spacing
    // Each line should have similar column structure
    size_t newlinePos = output.find('\n');
    if (newlinePos != std::string::npos) {
        std::string firstLine = output.substr(0, newlinePos);
        // First line should have at least id, value, op columns
        EXPECT_GT(firstLine.length(), 20U);
    }
}

