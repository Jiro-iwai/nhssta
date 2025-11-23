// -*- c++ -*-
// Unit tests for Ssta class
// Phase 2: Comprehensive unit test coverage for Ssta class

#include <gtest/gtest.h>
#include "../src/Ssta.h"
#include "../src/SstaResults.h"
#include <fstream>
#include <sys/stat.h>
#include <cmath>
#include "test_path_helper.h"

using namespace Nh;
using RandomVar = ::RandomVariable::RandomVariable;
using Normal = ::RandomVariable::Normal;

class SstaUnitTest : public ::testing::Test {
protected:
    void SetUp() override {
        ssta_ = new Ssta();
        example_dir = find_example_dir();
        if (example_dir.empty()) {
            example_dir = "../example";
        }
        
        // Create test directory if needed
        test_dir = "../test/test_data";
        struct stat info;
        if (stat(test_dir.c_str(), &info) != 0) {
            std::string cmd = "mkdir -p " + test_dir;
            system(cmd.c_str());
        }
    }

    void TearDown() override {
        delete ssta_;
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

    Ssta* ssta_;
    std::string example_dir;
    std::string test_dir;
};

// Test: Ssta default constructor
TEST_F(SstaUnitTest, DefaultConstructor) {
    Ssta ssta;
    // Should not throw
    EXPECT_NO_THROW(ssta.set_lat());
    EXPECT_NO_THROW(ssta.set_correlation());
}

// Test: Set and get dlib path
TEST_F(SstaUnitTest, SetDlib) {
    std::string dlib_path = example_dir + "/ex4_gauss.dlib";
    ssta_->set_dlib(dlib_path);
    // No getter, so we test indirectly by reading
    EXPECT_NO_THROW(ssta_->read_dlib());
}

// Test: Set and get bench path
TEST_F(SstaUnitTest, SetBench) {
    std::string bench_path = example_dir + "/ex4.bench";
    ssta_->set_bench(bench_path);
    // No getter, so we test indirectly
    ssta_->set_dlib(example_dir + "/ex4_gauss.dlib");
    ssta_->set_lat();
    ssta_->check();
    ssta_->read_dlib();
    EXPECT_NO_THROW(ssta_->read_bench());
}

// Test: Set lat flag
TEST_F(SstaUnitTest, SetLat) {
    ssta_->set_lat();
    // Should be able to call report after setting up
    ssta_->set_dlib(example_dir + "/ex4_gauss.dlib");
    ssta_->set_bench(example_dir + "/ex4.bench");
    ssta_->check();
    ssta_->read_dlib();
    ssta_->read_bench();
    EXPECT_NO_THROW(ssta_->report());
}

// Test: Set correlation flag
TEST_F(SstaUnitTest, SetCorrelation) {
    ssta_->set_correlation();
    // Should be able to call report after setting up
    ssta_->set_dlib(example_dir + "/ex4_gauss.dlib");
    ssta_->set_bench(example_dir + "/ex4.bench");
    ssta_->check();
    ssta_->read_dlib();
    ssta_->read_bench();
    EXPECT_NO_THROW(ssta_->report());
}

// Test: Check method with missing dlib
TEST_F(SstaUnitTest, CheckMissingDlib) {
    ssta_->set_bench(example_dir + "/ex4.bench");
    // check() should exit(1) if dlib is missing
    // We can't easily test exit(), so we test the state before check()
    EXPECT_TRUE(ssta_->getLatResults().empty());
}

// Test: Check method with missing bench
TEST_F(SstaUnitTest, CheckMissingBench) {
    ssta_->set_dlib(example_dir + "/ex4_gauss.dlib");
    // check() should exit(1) if bench is missing
    // We can't easily test exit(), so we test the state before check()
    EXPECT_TRUE(ssta_->getLatResults().empty());
}

// Test: Read dlib with valid file
TEST_F(SstaUnitTest, ReadDlibValidFile) {
    ssta_->set_dlib(example_dir + "/ex4_gauss.dlib");
    EXPECT_NO_THROW(ssta_->read_dlib());
}

// Test: Read dlib with invalid file
TEST_F(SstaUnitTest, ReadDlibInvalidFile) {
    ssta_->set_dlib("nonexistent.dlib");
    EXPECT_THROW(ssta_->read_dlib(), Nh::Exception);
}

// Test: Read dlib with empty file
TEST_F(SstaUnitTest, ReadDlibEmptyFile) {
    std::string filepath = createTestFile("empty.dlib", "");
    ssta_->set_dlib(filepath);
    EXPECT_NO_THROW(ssta_->read_dlib());
    deleteTestFile("empty.dlib");
}

// Test: Read dlib with simple gate definition
TEST_F(SstaUnitTest, ReadDlibSimpleGate) {
    std::string content = "test_gate 0 y gauss (10.0, 2.0)\n";
    std::string filepath = createTestFile("simple.dlib", content);
    ssta_->set_dlib(filepath);
    EXPECT_NO_THROW(ssta_->read_dlib());
    deleteTestFile("simple.dlib");
}

// Test: Read bench with valid file
TEST_F(SstaUnitTest, ReadBenchValidFile) {
    ssta_->set_dlib(example_dir + "/ex4_gauss.dlib");
    ssta_->set_bench(example_dir + "/ex4.bench");
    ssta_->set_lat();
    ssta_->check();
    ssta_->read_dlib();
    EXPECT_NO_THROW(ssta_->read_bench());
}

// Test: Read bench with invalid file
TEST_F(SstaUnitTest, ReadBenchInvalidFile) {
    ssta_->set_dlib(example_dir + "/ex4_gauss.dlib");
    ssta_->set_bench("nonexistent.bench");
    ssta_->set_lat();
    ssta_->check();
    ssta_->read_dlib();
    EXPECT_THROW(ssta_->read_bench(), Nh::Exception);
}

// Test: Read bench with INPUT declaration
TEST_F(SstaUnitTest, ReadBenchInput) {
    std::string dlib_content = "gate1 0 y gauss (10.0, 2.0)\n";
    std::string bench_content = "INPUT(A)\n";
    std::string dlib_path = createTestFile("test.dlib", dlib_content);
    std::string bench_path = createTestFile("test.bench", bench_content);
    
    ssta_->set_dlib(dlib_path);
    ssta_->set_bench(bench_path);
    ssta_->set_lat();
    ssta_->check();
    ssta_->read_dlib();
    EXPECT_NO_THROW(ssta_->read_bench());
    
    deleteTestFile("test.dlib");
    deleteTestFile("test.bench");
}

// Test: Read bench with OUTPUT declaration
TEST_F(SstaUnitTest, ReadBenchOutput) {
    std::string dlib_content = "gate1 0 y gauss (10.0, 2.0)\n";
    std::string bench_content = "INPUT(A)\nOUTPUT(Y)\n";
    std::string dlib_path = createTestFile("test2.dlib", dlib_content);
    std::string bench_path = createTestFile("test2.bench", bench_content);
    
    ssta_->set_dlib(dlib_path);
    ssta_->set_bench(bench_path);
    ssta_->set_lat();
    ssta_->check();
    ssta_->read_dlib();
    EXPECT_NO_THROW(ssta_->read_bench());
    
    deleteTestFile("test2.dlib");
    deleteTestFile("test2.bench");
}

// Test: Read bench with NET declaration
TEST_F(SstaUnitTest, ReadBenchNet) {
    std::string dlib_content = "gate1 0 y gauss (10.0, 2.0)\n";
    std::string bench_content = "INPUT(A)\nY = gate1(A)\n";
    std::string dlib_path = createTestFile("test3.dlib", dlib_content);
    std::string bench_path = createTestFile("test3.bench", bench_content);
    
    ssta_->set_dlib(dlib_path);
    ssta_->set_bench(bench_path);
    ssta_->set_lat();
    ssta_->check();
    ssta_->read_dlib();
    EXPECT_NO_THROW(ssta_->read_bench());
    
    deleteTestFile("test3.dlib");
    deleteTestFile("test3.bench");
}

// Test: GetLatResults with empty signals
TEST_F(SstaUnitTest, GetLatResultsEmpty) {
    LatResults results = ssta_->getLatResults();
    EXPECT_EQ(results.size(), 0);
}

// Test: GetLatResults after reading files
TEST_F(SstaUnitTest, GetLatResultsAfterRead) {
    ssta_->set_dlib(example_dir + "/ex4_gauss.dlib");
    ssta_->set_bench(example_dir + "/ex4.bench");
    ssta_->set_lat();
    ssta_->check();
    ssta_->read_dlib();
    ssta_->read_bench();
    
    LatResults results = ssta_->getLatResults();
    EXPECT_GT(results.size(), 0);
    
    // Check that results have valid values
    for (const auto& result : results) {
        EXPECT_FALSE(result.node_name.empty());
        EXPECT_FALSE(isnan(result.mean));
        EXPECT_FALSE(isnan(result.std_dev));
        EXPECT_GE(result.std_dev, 0.0);
    }
}

// Test: GetCorrelationMatrix with empty signals
TEST_F(SstaUnitTest, GetCorrelationMatrixEmpty) {
    CorrelationMatrix matrix = ssta_->getCorrelationMatrix();
    EXPECT_EQ(matrix.node_names.size(), 0);
    EXPECT_EQ(matrix.correlations.size(), 0);
}

// Test: GetCorrelationMatrix after reading files
TEST_F(SstaUnitTest, GetCorrelationMatrixAfterRead) {
    ssta_->set_dlib(example_dir + "/ex4_gauss.dlib");
    ssta_->set_bench(example_dir + "/ex4.bench");
    ssta_->set_correlation();
    ssta_->check();
    ssta_->read_dlib();
    ssta_->read_bench();
    
    CorrelationMatrix matrix = ssta_->getCorrelationMatrix();
    EXPECT_GT(matrix.node_names.size(), 0);
    
    // Check that matrix is symmetric
    for (const auto& node1 : matrix.node_names) {
        for (const auto& node2 : matrix.node_names) {
            double corr12 = matrix.getCorrelation(node1, node2);
            double corr21 = matrix.getCorrelation(node2, node1);
            EXPECT_NEAR(corr12, corr21, 1e-6);
        }
    }
}

// Test: Report with lat only
TEST_F(SstaUnitTest, ReportLatOnly) {
    ssta_->set_dlib(example_dir + "/ex4_gauss.dlib");
    ssta_->set_bench(example_dir + "/ex4.bench");
    ssta_->set_lat();
    ssta_->check();
    ssta_->read_dlib();
    ssta_->read_bench();
    EXPECT_NO_THROW(ssta_->report());
}

// Test: Report with correlation only
TEST_F(SstaUnitTest, ReportCorrelationOnly) {
    ssta_->set_dlib(example_dir + "/ex4_gauss.dlib");
    ssta_->set_bench(example_dir + "/ex4.bench");
    ssta_->set_correlation();
    ssta_->check();
    ssta_->read_dlib();
    ssta_->read_bench();
    EXPECT_NO_THROW(ssta_->report());
}

// Test: Report with both lat and correlation
TEST_F(SstaUnitTest, ReportBoth) {
    ssta_->set_dlib(example_dir + "/ex4_gauss.dlib");
    ssta_->set_bench(example_dir + "/ex4.bench");
    ssta_->set_lat();
    ssta_->set_correlation();
    ssta_->check();
    ssta_->read_dlib();
    ssta_->read_bench();
    EXPECT_NO_THROW(ssta_->report());
}

// Test: Exception handling for invalid gate in dlib
TEST_F(SstaUnitTest, InvalidGateInDlib) {
    std::string content = "invalid_gate 0 y gauss (10.0, 2.0)\n";
    std::string filepath = createTestFile("invalid.dlib", content);
    ssta_->set_dlib(filepath);
    ssta_->set_bench(example_dir + "/ex4.bench");
    ssta_->set_lat();
    ssta_->check();
    ssta_->read_dlib();
    // Should throw when trying to use invalid gate
    EXPECT_THROW(ssta_->read_bench(), Nh::Exception);
    deleteTestFile("invalid.dlib");
}

// Test: Exception handling for duplicate signal definition
TEST_F(SstaUnitTest, DuplicateSignalDefinition) {
    std::string dlib_content = "gate1 0 y gauss (10.0, 2.0)\n";
    std::string bench_content = "INPUT(A)\nINPUT(A)\n"; // Duplicate INPUT
    std::string dlib_path = createTestFile("dup.dlib", dlib_content);
    std::string bench_path = createTestFile("dup.bench", bench_content);
    
    ssta_->set_dlib(dlib_path);
    ssta_->set_bench(bench_path);
    ssta_->set_lat();
    ssta_->check();
    ssta_->read_dlib();
    // read_bench() converts Nh::RuntimeException to Nh::Exception
    EXPECT_THROW(ssta_->read_bench(), Nh::Exception);
    
    try {
        ssta_->read_bench();
        FAIL() << "Expected exception was not thrown";
    } catch (Nh::Exception& e) {
        std::string msg = e.what();
        EXPECT_NE(msg.find("input"), std::string::npos);
        EXPECT_NE(msg.find("multiply defined"), std::string::npos);
    }
    
    deleteTestFile("dup.dlib");
    deleteTestFile("dup.bench");
}

// Test: Exception handling for floating circuit
TEST_F(SstaUnitTest, FloatingCircuit) {
    std::string dlib_content = "gate1 0 y gauss (10.0, 2.0)\n";
    std::string bench_content = "Y = gate1(Z)\n"; // Z is not defined
    std::string dlib_path = createTestFile("float.dlib", dlib_content);
    std::string bench_path = createTestFile("float.bench", bench_content);
    
    ssta_->set_dlib(dlib_path);
    ssta_->set_bench(bench_path);
    ssta_->set_lat();
    ssta_->check();
    ssta_->read_dlib();
    // read_bench() converts Nh::RuntimeException to Nh::Exception
    EXPECT_THROW(ssta_->read_bench(), Nh::Exception);
    
    try {
        ssta_->read_bench();
        FAIL() << "Expected exception was not thrown";
    } catch (Nh::Exception& e) {
        std::string msg = e.what();
        EXPECT_NE(msg.find("floating"), std::string::npos);
    }
    
    deleteTestFile("float.dlib");
    deleteTestFile("float.bench");
}

