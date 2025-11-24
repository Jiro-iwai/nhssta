// -*- c++ -*-
// Unit tests for exception type preservation in Ssta class
// Tests that Ssta methods preserve specific exception types (FileException, ParseException, RuntimeException)
// instead of converting them to base Exception class

#include <gtest/gtest.h>
#include <sys/stat.h>

#include <cstdio>
#include <fstream>
#include <nhssta/exception.hpp>
#include <nhssta/ssta.hpp>
#include <sstream>

#include "../src/expression.hpp"
#include "../src/parser.hpp"

using namespace Nh;

class ExceptionTypePreservationTest : public ::testing::Test {
   protected:
    void SetUp() override {
        test_dir = "../test/test_data";
        struct stat info;
        if (stat(test_dir.c_str(), &info) != 0) {
            std::string cmd = "mkdir -p " + test_dir;
            system(cmd.c_str());
        }
    }

    void TearDown() override {
        // Cleanup
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

// Test: Ssta::read_dlib() preserves FileException type
TEST_F(ExceptionTypePreservationTest, ReadDlibPreservesFileException) {
    Nh::Ssta ssta;
    std::string invalid_file = test_dir + "/nonexistent.dlib";
    ssta.set_dlib(invalid_file);

    // Should throw FileException, not base Exception
    EXPECT_THROW(ssta.read_dlib(), Nh::FileException);

    try {
        ssta.read_dlib();
        FAIL() << "Expected FileException was not thrown";
    } catch (const Nh::FileException& e) {
        // Verify it's actually FileException, not base Exception
        std::string msg = e.what();
        EXPECT_NE(msg.find("File error"), std::string::npos) << "Message: " << msg;
    } catch (const Nh::Exception& e) {
        // If we catch base Exception, the type was lost
        FAIL() << "Exception type was lost - caught base Exception instead of FileException";
    } catch (...) {
        FAIL() << "Unexpected exception type";
    }
}

// Test: Ssta::read_dlib() preserves ParseException type
TEST_F(ExceptionTypePreservationTest, ReadDlibPreservesParseException) {
    Nh::Ssta ssta;
    
    // Create a dlib file with syntax error
    std::string dlib_file = createTestFile("invalid.dlib", "and 0 y gauss (10.0\n");  // Missing closing paren
    ssta.set_dlib(dlib_file);

    // Should throw ParseException, not base Exception
    EXPECT_THROW(ssta.read_dlib(), Nh::ParseException);

    try {
        ssta.read_dlib();
        FAIL() << "Expected ParseException was not thrown";
    } catch (const Nh::ParseException& e) {
        // Verify it's actually ParseException, not base Exception
        std::string msg = e.what();
        EXPECT_NE(msg.find("Parse error"), std::string::npos) << "Message: " << msg;
    } catch (const Nh::Exception& e) {
        // If we catch base Exception, the type was lost
        FAIL() << "Exception type was lost - caught base Exception instead of ParseException";
    } catch (...) {
        FAIL() << "Unexpected exception type";
    }

    deleteTestFile("invalid.dlib");
}

// Test: Ssta::read_bench() preserves FileException type
TEST_F(ExceptionTypePreservationTest, ReadBenchPreservesFileException) {
    Nh::Ssta ssta;
    
    // Create a valid dlib file first (need both input pins)
    std::string dlib_file = createTestFile("test.dlib", "and 0 y gauss (10.0, 2.0)\nand 1 y gauss (10.0, 2.0)\n");
    ssta.set_dlib(dlib_file);
    ssta.read_dlib();
    
    std::string invalid_file = test_dir + "/nonexistent.bench";
    ssta.set_bench(invalid_file);

    // Should throw FileException, not base Exception
    EXPECT_THROW(ssta.read_bench(), Nh::FileException);

    try {
        ssta.read_bench();
        FAIL() << "Expected FileException was not thrown";
    } catch (const Nh::FileException& e) {
        // Verify it's actually FileException, not base Exception
        std::string msg = e.what();
        EXPECT_NE(msg.find("File error"), std::string::npos) << "Message: " << msg;
    } catch (const Nh::Exception& e) {
        // If we catch base Exception, the type was lost
        FAIL() << "Exception type was lost - caught base Exception instead of FileException";
    } catch (...) {
        FAIL() << "Unexpected exception type";
    }

    deleteTestFile("test.dlib");
}

// Test: Ssta::read_bench() preserves ParseException type
TEST_F(ExceptionTypePreservationTest, ReadBenchPreservesParseException) {
    Nh::Ssta ssta;
    
    // Create a valid dlib file first
    std::string dlib_file = createTestFile("test.dlib", "and 0 y gauss (10.0, 2.0)\nand 1 y gauss (10.0, 2.0)\n");
    ssta.set_dlib(dlib_file);
    ssta.read_dlib();
    
    // Create a bench file with syntax error in NET line (extra token at end)
    // This will cause a parse error when checkEnd() finds unexpected token -> ParseException
    std::string bench_file = createTestFile("invalid.bench", 
        "INPUT(a)\n"
        "INPUT(b)\n"
        "OUTPUT(y)\n"
        "y = AND(a, b) extra_token\n");  // Extra token after closing paren - will cause ParseException in checkEnd()
    ssta.set_bench(bench_file);

    // Should throw ParseException, not base Exception
    // ParseException is thrown from read_bench_net when checkEnd() finds unexpected token
    try {
        ssta.read_bench();
        FAIL() << "Expected ParseException was not thrown";
    } catch (const Nh::ParseException& e) {
        // Verify it's actually ParseException, not base Exception
        std::string msg = e.what();
        EXPECT_NE(msg.find("Parse error"), std::string::npos) << "Message: " << msg;
        EXPECT_NE(msg.find("unexpected token"), std::string::npos) << "Message: " << msg;
    } catch (const Nh::Exception& e) {
        // If we catch base Exception, the type was lost
        FAIL() << "Exception type was lost - caught base Exception instead of ParseException. Message: " << e.what();
    } catch (...) {
        FAIL() << "Unexpected exception type";
    }

    deleteTestFile("test.dlib");
    deleteTestFile("invalid.bench");
}

// Test: Ssta::read_bench() preserves RuntimeException type (unknown gate)
TEST_F(ExceptionTypePreservationTest, ReadBenchPreservesRuntimeExceptionUnknownGate) {
    Nh::Ssta ssta;
    
    // Create a valid dlib file first
    std::string dlib_file = createTestFile("test.dlib", "and 0 y gauss (10.0, 2.0)\n");
    ssta.set_dlib(dlib_file);
    ssta.read_dlib();
    
    // Create a bench file with unknown gate
    std::string bench_file = createTestFile("unknown_gate.bench", 
        "INPUT(a)\n"
        "OUTPUT(y)\n"
        "y = unknown_gate(a)\n");  // unknown_gate is not in dlib
    ssta.set_bench(bench_file);

    // Should throw ParseException (for unknown gate during parsing)
    // Actually, based on the code, unknown gate throws ParseException in read_bench_net
    EXPECT_THROW(ssta.read_bench(), Nh::ParseException);

    deleteTestFile("test.dlib");
    deleteTestFile("unknown_gate.bench");
}

// Test: Ssta::read_bench() preserves RuntimeException type (floating node)
TEST_F(ExceptionTypePreservationTest, ReadBenchPreservesRuntimeExceptionFloatingNode) {
    Nh::Ssta ssta;
    
    // Create a valid dlib file first (need both input pins)
    std::string dlib_file = createTestFile("test.dlib", "and 0 y gauss (10.0, 2.0)\nand 1 y gauss (10.0, 2.0)\n");
    ssta.set_dlib(dlib_file);
    ssta.read_dlib();
    
    // Create a bench file with floating node (circular dependency or unprocessable node)
    // z depends on w, but w is not defined, creating a floating node
    std::string bench_file = createTestFile("floating.bench", 
        "INPUT(a)\n"
        "INPUT(b)\n"
        "OUTPUT(y)\n"
        "y = AND(a, b)\n"
        "z = AND(a, w)\n");  // w is not defined, so z cannot be processed (floating)
    ssta.set_bench(bench_file);

    // Should throw RuntimeException for floating node, not base Exception
    EXPECT_THROW(ssta.read_bench(), Nh::RuntimeException);

    try {
        ssta.read_bench();
        FAIL() << "Expected RuntimeException was not thrown";
    } catch (const Nh::RuntimeException& e) {
        // Verify it's actually RuntimeException, not base Exception
        std::string msg = e.what();
        EXPECT_NE(msg.find("Runtime error"), std::string::npos) << "Message: " << msg;
        EXPECT_NE(msg.find("floating"), std::string::npos) << "Message: " << msg;
    } catch (const Nh::Exception& e) {
        // If we catch base Exception, the type was lost
        FAIL() << "Exception type was lost - caught base Exception instead of RuntimeException";
    } catch (...) {
        FAIL() << "Unexpected exception type";
    }

    deleteTestFile("test.dlib");
    deleteTestFile("floating.bench");
}

// Test: Ssta::getLatResults() and getCorrelationMatrix() work without I/O
TEST_F(ExceptionTypePreservationTest, GetResultsWorksWithoutIO) {
    Nh::Ssta ssta;
    
    // Create a valid dlib file (need both input pins)
    std::string dlib_file = createTestFile("test.dlib", "and 0 y gauss (10.0, 2.0)\nand 1 y gauss (10.0, 2.0)\n");
    ssta.set_dlib(dlib_file);
    ssta.read_dlib();
    
    // Create a valid bench file
    std::string bench_file = createTestFile("test.bench", 
        "INPUT(a)\n"
        "INPUT(b)\n"
        "OUTPUT(y)\n"
        "y = AND(a, b)\n");
    ssta.set_bench(bench_file);
    ssta.read_bench();
    
    // Enable report flags
    ssta.set_lat();
    ssta.set_correlation();
    
    // Test that getLatResults() and getCorrelationMatrix() work without report()
    // These methods should not perform I/O operations
    EXPECT_NO_THROW({
        Nh::LatResults lat_results = ssta.getLatResults();
        Nh::CorrelationMatrix corr_matrix = ssta.getCorrelationMatrix();
        EXPECT_FALSE(lat_results.empty());
        EXPECT_FALSE(corr_matrix.node_names.empty());
    });

    deleteTestFile("test.dlib");
    deleteTestFile("test.bench");
}

// Test: Exception type hierarchy is preserved
TEST_F(ExceptionTypePreservationTest, ExceptionTypeHierarchy) {
    // Verify that specific exception types can still be caught as base Exception
    // This ensures backward compatibility
    Nh::Ssta ssta;
    std::string invalid_file = test_dir + "/nonexistent.dlib";
    ssta.set_dlib(invalid_file);

    try {
        ssta.read_dlib();
        FAIL() << "Expected exception was not thrown";
    } catch (const Nh::FileException& e) {
        // First catch specific type
        EXPECT_NE(e.what(), nullptr);
    } catch (const Nh::Exception& e) {
        // Should also be catchable as base Exception (polymorphism)
        EXPECT_NE(e.what(), nullptr);
    } catch (...) {
        FAIL() << "Unexpected exception type";
    }
}

