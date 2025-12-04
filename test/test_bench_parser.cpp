// -*- c++ -*-
// Unit tests for BenchParser class
// Phase 1: File parser separation

#include <gtest/gtest.h>
#include <sys/stat.h>

#include <cstdio>
#include <fstream>
#include <sstream>

#include "../src/bench_parser.hpp"
#include <nhssta/net_line.hpp>

using Nh::BenchParser;

class BenchParserTest : public ::testing::Test {
   protected:
    void SetUp() override {
        test_dir = "../test/test_data";
        // Create test directory if it doesn't exist
        struct stat info;
        if (stat(test_dir.c_str(), &info) != 0) {
            // Directory doesn't exist, create it
            std::string cmd = "mkdir -p " + test_dir;
            system(cmd.c_str());
        }
    }

    void TearDown() override {
        // Cleanup test files
    }

    std::string createTestFile(const std::string& filename, const std::string& content) {
        std::string filepath = test_dir + "/" + filename;
        std::ofstream file(filepath);
        if (file.is_open()) {
            file << content;
            file.close();
        }
        return filepath;
    }

    void deleteTestFile(const std::string& filename) {
        std::string filepath = test_dir + "/" + filename;
        remove(filepath.c_str());
    }

    std::string test_dir;
};

// Test: BenchParser with simple INPUT/OUTPUT
TEST_F(BenchParserTest, ParseSimpleInputOutput) {
    std::string content = 
        "INPUT(A)\n"
        "OUTPUT(Y)\n";
    std::string filepath = createTestFile("test_simple.bench", content);

    BenchParser parser(filepath);
    parser.parse();

    const auto& inputs = parser.inputs();
    const auto& outputs = parser.outputs();
    
    EXPECT_EQ(inputs.size(), 1);
    EXPECT_NE(inputs.find("A"), inputs.end());
    
    EXPECT_EQ(outputs.size(), 1);
    EXPECT_NE(outputs.find("Y"), outputs.end());
    
    EXPECT_EQ(parser.dff_outputs().size(), 0);
    EXPECT_EQ(parser.dff_inputs().size(), 0);
    EXPECT_EQ(parser.net().size(), 0);

    deleteTestFile("test_simple.bench");
}

// Test: BenchParser with NET definition
TEST_F(BenchParserTest, ParseNet) {
    std::string content = 
        "INPUT(A)\n"
        "OUTPUT(Y)\n"
        "Y = INV(A)\n";
    std::string filepath = createTestFile("test_net.bench", content);

    BenchParser parser(filepath);
    parser.parse();

    const auto& inputs = parser.inputs();
    const auto& outputs = parser.outputs();
    const auto& net = parser.net();
    
    EXPECT_EQ(inputs.size(), 1);
    EXPECT_NE(inputs.find("A"), inputs.end());
    
    EXPECT_EQ(outputs.size(), 1);
    EXPECT_NE(outputs.find("Y"), outputs.end());
    
    EXPECT_EQ(net.size(), 1);
    auto it = net.begin();
    EXPECT_EQ((*it)->out(), "Y");
    EXPECT_EQ((*it)->gate(), "inv");

    deleteTestFile("test_net.bench");
}

// Test: BenchParser with multiple inputs and outputs
TEST_F(BenchParserTest, ParseMultipleInputsOutputs) {
    std::string content = 
        "INPUT(A)\n"
        "INPUT(B)\n"
        "INPUT(C)\n"
        "OUTPUT(Y)\n"
        "OUTPUT(Z)\n";
    std::string filepath = createTestFile("test_multiple.bench", content);

    BenchParser parser(filepath);
    parser.parse();

    const auto& inputs = parser.inputs();
    const auto& outputs = parser.outputs();
    
    EXPECT_EQ(inputs.size(), 3);
    EXPECT_NE(inputs.find("A"), inputs.end());
    EXPECT_NE(inputs.find("B"), inputs.end());
    EXPECT_NE(inputs.find("C"), inputs.end());
    
    EXPECT_EQ(outputs.size(), 2);
    EXPECT_NE(outputs.find("Y"), outputs.end());
    EXPECT_NE(outputs.find("Z"), outputs.end());

    deleteTestFile("test_multiple.bench");
}

// Test: BenchParser with DFF
TEST_F(BenchParserTest, ParseDFF) {
    std::string content = 
        "INPUT(CLK)\n"
        "OUTPUT(Q)\n"
        "Q = DFF(D, CLK)\n";
    std::string filepath = createTestFile("test_dff.bench", content);

    BenchParser parser(filepath);
    parser.parse();

    const auto& inputs = parser.inputs();
    const auto& outputs = parser.outputs();
    const auto& dff_outputs = parser.dff_outputs();
    const auto& dff_inputs = parser.dff_inputs();
    const auto& net = parser.net();
    
    EXPECT_EQ(inputs.size(), 1);
    EXPECT_NE(inputs.find("CLK"), inputs.end());
    
    EXPECT_EQ(outputs.size(), 1);
    EXPECT_NE(outputs.find("Q"), outputs.end());
    
    EXPECT_EQ(dff_outputs.size(), 1);
    EXPECT_NE(dff_outputs.find("Q"), dff_outputs.end());
    
    EXPECT_EQ(dff_inputs.size(), 1);
    EXPECT_NE(dff_inputs.find("D"), dff_inputs.end());
    
    // DFF should not be in net (handled separately)
    EXPECT_EQ(net.size(), 0);

    deleteTestFile("test_dff.bench");
}

// Test: BenchParser with comments
TEST_F(BenchParserTest, ParseWithComments) {
    std::string content = 
        "# This is a comment\n"
        "INPUT(A)\n"
        "# Another comment\n"
        "OUTPUT(Y)\n";
    std::string filepath = createTestFile("test_comments.bench", content);

    BenchParser parser(filepath);
    parser.parse();

    const auto& inputs = parser.inputs();
    const auto& outputs = parser.outputs();
    
    EXPECT_EQ(inputs.size(), 1);
    EXPECT_EQ(outputs.size(), 1);

    deleteTestFile("test_comments.bench");
}

// Test: BenchParser with complex circuit
TEST_F(BenchParserTest, ParseComplexCircuit) {
    std::string content = 
        "INPUT(A)\n"
        "INPUT(B)\n"
        "INPUT(C)\n"
        "OUTPUT(Y)\n"
        "N1 = NAND(A, N2)\n"
        "N2 = INV(B)\n"
        "N3 = INV(N1)\n"
        "N4 = OR(N2, C)\n"
        "Y = AND(N3, N4)\n";
    std::string filepath = createTestFile("test_complex.bench", content);

    BenchParser parser(filepath);
    parser.parse();

    const auto& inputs = parser.inputs();
    const auto& outputs = parser.outputs();
    const auto& net = parser.net();
    
    EXPECT_EQ(inputs.size(), 3);
    EXPECT_EQ(outputs.size(), 1);
    EXPECT_EQ(net.size(), 5);  // 5 NET lines

    deleteTestFile("test_complex.bench");
}

// Test: BenchParser with invalid file (should throw)
TEST_F(BenchParserTest, ParseInvalidFile) {
    std::string filepath = test_dir + "/nonexistent.bench";
    
    EXPECT_THROW({
        BenchParser parser(filepath);
        parser.parse();
    }, Nh::FileException);
}

// Test: BenchParser with duplicate INPUT (should throw)
TEST_F(BenchParserTest, ParseDuplicateInput) {
    std::string content = 
        "INPUT(A)\n"
        "INPUT(A)\n";
    std::string filepath = createTestFile("test_duplicate_input.bench", content);

    BenchParser parser(filepath);
    EXPECT_THROW(parser.parse(), Nh::RuntimeException);

    deleteTestFile("test_duplicate_input.bench");
}

// Test: BenchParser with duplicate OUTPUT (should throw)
TEST_F(BenchParserTest, ParseDuplicateOutput) {
    std::string content = 
        "OUTPUT(Y)\n"
        "OUTPUT(Y)\n";
    std::string filepath = createTestFile("test_duplicate_output.bench", content);

    BenchParser parser(filepath);
    EXPECT_THROW(parser.parse(), Nh::RuntimeException);

    deleteTestFile("test_duplicate_output.bench");
}

