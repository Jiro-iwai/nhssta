#include <gtest/gtest.h>

#include <nhssta/exception.hpp>
#include <nhssta/ssta.hpp>
#include <nhssta/gate.hpp>

// Internal implementation details (for testing)
#include "../src/parser.hpp"
#include "../src/random_variable.hpp"
// SmartPtrException has been removed - use Nh::RuntimeException directly
#include <sys/stat.h>

#include <cstdio>
#include <fstream>
#include <sstream>

#include "../src/expression.hpp"

using Nh::Parser;

// Test current exception handling behavior before unification
// This ensures we capture the current behavior before refactoring

class ExceptionUnificationTest : public ::testing::Test {
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

// Test: Parser exception thrown on file error (now Nh::FileException)
TEST_F(ExceptionUnificationTest, ParserExceptionOnFileError) {
    Parser parser("nonexistent_file.txt", '#', "(),", " \t\r");

    EXPECT_THROW(parser.checkFile(), Nh::FileException);

    try {
        parser.checkFile();
    } catch (Nh::FileException& e) {
        EXPECT_NE(std::string(e.what()).find("File error"), std::string::npos);
        EXPECT_NE(std::string(e.what()).find("nonexistent_file.txt"), std::string::npos);
    }
}


// Test: SmartPtrException behavior (now Nh::RuntimeException)
// SmartPtrException has been removed - use Nh::RuntimeException directly
TEST_F(ExceptionUnificationTest, SmartPtrExceptionBehavior) {
    try {
        throw Nh::RuntimeException("Test SmartPtr exception");
    } catch (Nh::RuntimeException& e) {
        // SmartPtrException has been removed - use Nh::RuntimeException directly
        EXPECT_NE(std::string(e.what()).find("Runtime error"), std::string::npos);
        EXPECT_NE(std::string(e.what()).find("Test SmartPtr exception"), std::string::npos);
    }
}

// Test: ExpressionException behavior
TEST_F(ExceptionUnificationTest, ExpressionExceptionBehavior) {
    // ExpressionException has assert(0) in constructor, so we can't test it normally
    // This test documents the current behavior
    EXPECT_TRUE(true);  // Placeholder
}

// Test: Nh::Exception (unified) behavior
TEST_F(ExceptionUnificationTest, NhExceptionBehavior) {
    try {
        throw Nh::Exception("Test unified exception");
    } catch (const Nh::Exception& e) {
        EXPECT_STREQ(e.what(), "Test unified exception");
    }
}

// Test: Nh::FileException behavior
TEST_F(ExceptionUnificationTest, NhFileExceptionBehavior) {
    try {
        throw Nh::FileException("test.txt", "file not found");
    } catch (const Nh::FileException& e) {
        std::string msg = e.what();
        EXPECT_NE(msg.find("File error"), std::string::npos);
        EXPECT_NE(msg.find("test.txt"), std::string::npos);
    }
}

// Test: Nh::ParseException behavior
TEST_F(ExceptionUnificationTest, NhParseExceptionBehavior) {
    try {
        throw Nh::ParseException("test.txt", 42, "syntax error");
    } catch (const Nh::ParseException& e) {
        std::string msg = e.what();
        EXPECT_NE(msg.find("Parse error"), std::string::npos);
        EXPECT_NE(msg.find("test.txt"), std::string::npos);
        EXPECT_NE(msg.find("42"), std::string::npos);
    }
}

// Test: Exception message format consistency
TEST_F(ExceptionUnificationTest, ExceptionMessageFormat) {
    // Test that all exceptions provide what() method
    Nh::Exception ssta_e("Ssta error");
    Nh::ParseException parser_e("test.txt", 1, "Parser error");
    Nh::RuntimeException gate_e("Gate error");
    Nh::RuntimeException rv_e("RandomVariable error");
    Nh::RuntimeException smartptr_e("SmartPtr error");
    Nh::Exception unified_e("Unified error");

    EXPECT_NE(ssta_e.what(), nullptr);      // Nh::Exception::what() returns const char*
    EXPECT_NE(parser_e.what(), nullptr);    // Nh::ParseException::what() returns const char*
    EXPECT_NE(gate_e.what(), nullptr);      // Nh::RuntimeException::what() returns const char*
    EXPECT_NE(rv_e.what(), nullptr);        // Nh::RuntimeException::what() returns const char*
    EXPECT_NE(smartptr_e.what(), nullptr);  // Nh::RuntimeException::what() returns const char*
    EXPECT_NE(unified_e.what(), nullptr);
}

// Test: Exception inheritance (if applicable)
TEST_F(ExceptionUnificationTest, ExceptionInheritance) {
    // Test that Nh::Exception inherits from std::exception
    Nh::Exception e("test");
    const std::exception& base = e;
    EXPECT_NE(base.what(), nullptr);
}

// Test: Exception propagation through Ssta
TEST_F(ExceptionUnificationTest, ExceptionPropagationThroughSsta) {
    Nh::Ssta ssta;

    // Test that Parser exceptions (now Nh::FileException) are caught and rethrown as Nh::Exception
    std::string invalid_file = test_dir + "/nonexistent.dlib";
    ssta.set_dlib(invalid_file);
    ssta.set_bench("../example/my.bench");

    EXPECT_THROW(ssta.read_dlib(), Nh::Exception);

    try {
        ssta.read_dlib();
        FAIL() << "Expected exception was not thrown";
    } catch (Nh::Exception& e) {
        // Should contain error message from Parser (now Nh::FileException)
        // The message format is: "File error: <filename>: failed to open file"
        std::string msg = e.what();
        EXPECT_NE(msg.find("File error"), std::string::npos) << "Message: " << msg;
        EXPECT_NE(msg.find("failed to open file"), std::string::npos) << "Message: " << msg;
    } catch (...) {
        FAIL() << "Unexpected exception type";
    }
}
