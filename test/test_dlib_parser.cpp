// -*- c++ -*-
// Unit tests for DlibParser class
// Phase 1: File parser separation

#include <gtest/gtest.h>
#include <sys/stat.h>

#include <cstdio>
#include <fstream>
#include <sstream>

#include "../src/dlib_parser.hpp"
#include <nhssta/gate.hpp>
#include "../src/normal.hpp"

using Nh::DlibParser;
using Nh::Gate;
using Normal = RandomVariable::Normal;

class DlibParserTest : public ::testing::Test {
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

// Test: DlibParser creation with valid file
TEST_F(DlibParserTest, ParseSimpleGate) {
    std::string content = "inv  0  y  gauss  (15.0, 2.0)\n";
    std::string filepath = createTestFile("test_simple.dlib", content);

    DlibParser parser(filepath);
    parser.parse();

    const auto& gates = parser.gates();
    EXPECT_EQ(gates.size(), 1);
    
    auto it = gates.find("inv");
    EXPECT_NE(it, gates.end());
    
    Gate gate = it->second;
    EXPECT_EQ(gate->type_name(), "inv");
    
    // Check delay is set correctly
    Normal delay = gate->delay("0", "y");
    EXPECT_DOUBLE_EQ(delay->mean(), 15.0);
    EXPECT_DOUBLE_EQ(delay->variance(), 4.0);  // sigma^2 = 2.0^2 = 4.0

    deleteTestFile("test_simple.dlib");
}

// Test: DlibParser with const delay
TEST_F(DlibParserTest, ParseConstDelay) {
    std::string content = "inv  0  y  const  (10.0)\n";
    std::string filepath = createTestFile("test_const.dlib", content);

    DlibParser parser(filepath);
    parser.parse();

    const auto& gates = parser.gates();
    EXPECT_EQ(gates.size(), 1);
    
    auto it = gates.find("inv");
    EXPECT_NE(it, gates.end());
    
    Gate gate = it->second;
    Normal delay = gate->delay("0", "y");
    EXPECT_DOUBLE_EQ(delay->mean(), 10.0);
    EXPECT_DOUBLE_EQ(delay->variance(), 0.0);  // const delay has zero variance

    deleteTestFile("test_const.dlib");
}

// Test: DlibParser with multiple gates
TEST_F(DlibParserTest, ParseMultipleGates) {
    std::string content = 
        "inv  0  y  gauss  (15.0, 2.0)\n"
        "nand  0  y  gauss  (24.0, 3.0)\n"
        "nand  1  y  gauss  (20.0, 3.0)\n";
    std::string filepath = createTestFile("test_multiple.dlib", content);

    DlibParser parser(filepath);
    parser.parse();

    const auto& gates = parser.gates();
    EXPECT_EQ(gates.size(), 2);  // inv and nand
    
    // Check inv gate
    auto inv_it = gates.find("inv");
    EXPECT_NE(inv_it, gates.end());
    Gate inv_gate = inv_it->second;
    Normal inv_delay = inv_gate->delay("0", "y");
    EXPECT_DOUBLE_EQ(inv_delay->mean(), 15.0);
    
    // Check nand gate (should have 2 delays)
    auto nand_it = gates.find("nand");
    EXPECT_NE(nand_it, gates.end());
    Gate nand_gate = nand_it->second;
    Normal nand_delay0 = nand_gate->delay("0", "y");
    EXPECT_DOUBLE_EQ(nand_delay0->mean(), 24.0);
    Normal nand_delay1 = nand_gate->delay("1", "y");
    EXPECT_DOUBLE_EQ(nand_delay1->mean(), 20.0);

    deleteTestFile("test_multiple.dlib");
}

// Test: DlibParser with comments
TEST_F(DlibParserTest, ParseWithComments) {
    std::string content = 
        "# This is a comment\n"
        "inv  0  y  gauss  (15.0, 2.0)\n"
        "# Another comment\n";
    std::string filepath = createTestFile("test_comments.dlib", content);

    DlibParser parser(filepath);
    parser.parse();

    const auto& gates = parser.gates();
    EXPECT_EQ(gates.size(), 1);
    
    auto it = gates.find("inv");
    EXPECT_NE(it, gates.end());

    deleteTestFile("test_comments.dlib");
}

// Test: DlibParser with invalid file (should throw)
TEST_F(DlibParserTest, ParseInvalidFile) {
    std::string filepath = test_dir + "/nonexistent.dlib";
    
    EXPECT_THROW({
        DlibParser parser(filepath);
        parser.parse();
    }, Nh::FileException);
}

// Test: DlibParser with invalid delay type (should throw)
TEST_F(DlibParserTest, ParseInvalidDelayType) {
    std::string content = "inv  0  y  invalid  (15.0, 2.0)\n";
    std::string filepath = createTestFile("test_invalid_type.dlib", content);

    DlibParser parser(filepath);
    EXPECT_THROW(parser.parse(), Nh::ParseException);

    deleteTestFile("test_invalid_type.dlib");
}

// Test: DlibParser with negative mean (should throw)
TEST_F(DlibParserTest, ParseNegativeMean) {
    std::string content = "inv  0  y  gauss  (-15.0, 2.0)\n";
    std::string filepath = createTestFile("test_negative_mean.dlib", content);

    DlibParser parser(filepath);
    EXPECT_THROW(parser.parse(), Nh::ParseException);

    deleteTestFile("test_negative_mean.dlib");
}

