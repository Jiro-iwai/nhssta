// -*- c++ -*-
// Unit tests for main.cpp exception handling
// Tests that main.cpp can handle specific exception types appropriately
// This verifies that exception type preservation enables better error handling

#include <gtest/gtest.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cstdio>
#include <fstream>
#include <nhssta/exception.hpp>
#include <nhssta/ssta.hpp>
#include <sstream>
#include <string>

#include "test_path_helper.h"

using namespace Nh;

class MainExceptionHandlingTest : public ::testing::Test {
   protected:
    void SetUp() override {
        test_dir = "../test/test_data";
        struct stat info;
        if (stat(test_dir.c_str(), &info) != 0) {
            std::string cmd = "mkdir -p " + test_dir;
            system(cmd.c_str());
        }
        
        nhssta_path = find_nhssta_path();
        if (nhssta_path.empty()) {
            nhssta_path = "../src/nhssta";  // Fallback
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

    // Run nhssta and capture stderr output
    std::string run_nhssta_get_stderr(const std::string& args) {
        std::string cmd = nhssta_path + " " + args + " 2>&1";

        FILE* pipe = popen(cmd.c_str(), "r");
        if (!pipe) {
            return "";
        }

        std::string result;
        char buffer[128];
        while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
            result += buffer;
        }
        pclose(pipe);

        return result;
    }

    // Run nhssta and return exit code
    int run_nhssta_get_exit_code(const std::string& args) {
        std::string cmd = nhssta_path + " " + args + " > /dev/null 2>&1";
        return system(cmd.c_str());
    }

    std::string test_dir;
    std::string nhssta_path;
};

// Test: FileException should be caught and handled appropriately
// This test verifies that FileException is preserved through Ssta and can be handled
TEST_F(MainExceptionHandlingTest, FileExceptionHandling) {
    Nh::Ssta ssta;
    std::string invalid_file = test_dir + "/nonexistent.dlib";
    ssta.set_dlib(invalid_file);

    // Verify that FileException is thrown (not base Exception)
    try {
        ssta.read_dlib();
        FAIL() << "Expected FileException was not thrown";
    } catch (const Nh::FileException& e) {
        // Verify error message format
        std::string msg = e.what();
        EXPECT_NE(msg.find("File error"), std::string::npos) << "Message: " << msg;
        EXPECT_NE(msg.find("nonexistent.dlib"), std::string::npos) << "Message: " << msg;
    } catch (const Nh::Exception& e) {
        // If we catch base Exception, the type was lost
        FAIL() << "Exception type was lost - caught base Exception instead of FileException";
    } catch (...) {
        FAIL() << "Unexpected exception type";
    }
}

// Test: ParseException should be caught and handled appropriately
TEST_F(MainExceptionHandlingTest, ParseExceptionHandling) {
    Nh::Ssta ssta;
    
    // Create a dlib file with syntax error
    std::string dlib_file = createTestFile("invalid.dlib", "and 0 y gauss (10.0\n");  // Missing closing paren
    ssta.set_dlib(dlib_file);

    // Verify that ParseException is thrown (not base Exception)
    try {
        ssta.read_dlib();
        FAIL() << "Expected ParseException was not thrown";
    } catch (const Nh::ParseException& e) {
        // Verify error message format
        std::string msg = e.what();
        EXPECT_NE(msg.find("Parse error"), std::string::npos) << "Message: " << msg;
        EXPECT_NE(msg.find("invalid.dlib"), std::string::npos) << "Message: " << msg;
    } catch (const Nh::Exception& e) {
        // If we catch base Exception, the type was lost
        FAIL() << "Exception type was lost - caught base Exception instead of ParseException";
    } catch (...) {
        FAIL() << "Unexpected exception type";
    }

    deleteTestFile("invalid.dlib");
}

// Test: RuntimeException should be caught and handled appropriately
TEST_F(MainExceptionHandlingTest, RuntimeExceptionHandling) {
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

    // Verify that RuntimeException is thrown (not base Exception)
    try {
        ssta.read_bench();
        FAIL() << "Expected RuntimeException was not thrown";
    } catch (const Nh::RuntimeException& e) {
        // Verify error message format
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

// Test: Exception type information enables better error messages in CLI
// This test verifies that error messages contain appropriate information
TEST_F(MainExceptionHandlingTest, ErrorMessageQuality) {
    // Test FileException error message
    std::string output = run_nhssta_get_stderr("-l -d nonexistent.dlib -b ../example/ex4.bench");
    EXPECT_NE(output.find("error"), std::string::npos) << "Should contain error message";
    EXPECT_NE(output.find("nonexistent.dlib"), std::string::npos) 
        << "Should mention the file name. Got: " << output.substr(0, 300);
}

// Test: All exception types can be caught as base Exception (polymorphism)
// This ensures backward compatibility
TEST_F(MainExceptionHandlingTest, ExceptionPolymorphism) {
    Nh::Ssta ssta;
    std::string invalid_file = test_dir + "/nonexistent.dlib";
    ssta.set_dlib(invalid_file);

    // Should be catchable as base Exception
    try {
        ssta.read_dlib();
        FAIL() << "Expected exception was not thrown";
    } catch (const Nh::Exception& e) {
        // This should work due to polymorphism
        EXPECT_NE(e.what(), nullptr);
        std::string msg = e.what();
        EXPECT_NE(msg.find("File error"), std::string::npos) << "Message: " << msg;
    } catch (...) {
        FAIL() << "Unexpected exception type";
    }
}

// Test: Exception type information is preserved through call chain
// This verifies that exceptions maintain their type through Ssta methods
TEST_F(MainExceptionHandlingTest, ExceptionTypePreservationThroughCallChain) {
    Nh::Ssta ssta;
    
    // Test FileException preservation
    std::string invalid_file = test_dir + "/nonexistent.dlib";
    ssta.set_dlib(invalid_file);
    
    bool caught_file_exception = false;
    bool caught_base_exception = false;
    
    try {
        ssta.read_dlib();
    } catch (const Nh::FileException& e) {
        caught_file_exception = true;
        EXPECT_NE(e.what(), nullptr);
    } catch (const Nh::Exception& e) {
        caught_base_exception = true;
        // If we catch base Exception here, the type was lost
    } catch (...) {
        FAIL() << "Unexpected exception type";
    }
    
    EXPECT_TRUE(caught_file_exception) 
        << "FileException should be preserved, not converted to base Exception";
    EXPECT_FALSE(caught_base_exception) 
        << "Should catch FileException directly, not base Exception";
}

