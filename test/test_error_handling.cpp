#include <gtest/gtest.h>
#include <cstdlib>
#include <cstdio>
#include <string>
#include <sys/stat.h>
#include <unistd.h>
#include "test_path_helper.h"

// Test current error handling behavior before refactoring
// This ensures we don't break existing error messages

class ErrorHandlingTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Use path helper to find nhssta executable
        nhssta_path = find_nhssta_path();
        if (nhssta_path.empty()) {
            nhssta_path = "../src/nhssta"; // Fallback
        }
    }

    std::string run_nhssta(const std::string& args) {
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

    std::string nhssta_path;
};

// Test: Missing -d option
TEST_F(ErrorHandlingTest, MissingDlibOption) {
    std::string output = run_nhssta("-l -b ../example/ex4.bench");
    
    // Should contain error message about -d
    EXPECT_NE(output.find("error"), std::string::npos);
    EXPECT_NE(output.find("-d"), std::string::npos);
}

// Test: Missing -b option
TEST_F(ErrorHandlingTest, MissingBenchOption) {
    std::string output = run_nhssta("-l -d ../example/ex4_gauss.dlib");
    
    // Should contain error message about -b
    EXPECT_NE(output.find("error"), std::string::npos);
    EXPECT_NE(output.find("-b"), std::string::npos);
}

// Test: Missing both options
TEST_F(ErrorHandlingTest, MissingBothOptions) {
    std::string output = run_nhssta("-l");
    
    // Should contain error messages
    EXPECT_NE(output.find("error"), std::string::npos);
}

// Test: Non-existent dlib file
TEST_F(ErrorHandlingTest, NonExistentDlibFile) {
    std::string output = run_nhssta("-l -d nonexistent.dlib -b ../example/ex4.bench");
    
    // Should contain error about file
    EXPECT_NE(output.find("error"), std::string::npos);
}

// Test: Non-existent bench file
TEST_F(ErrorHandlingTest, NonExistentBenchFile) {
    std::string output = run_nhssta("-l -d ../example/ex4_gauss.dlib -b nonexistent.bench");
    
    // Should contain error about file
    EXPECT_NE(output.find("error"), std::string::npos);
}

// Test: Invalid gate in dlib
TEST_F(ErrorHandlingTest, InvalidGateInDlib) {
    // Create a temporary invalid dlib file
    std::string temp_dlib = "/tmp/test_invalid.dlib";
    FILE* f = fopen(temp_dlib.c_str(), "w");
    if (f) {
        fprintf(f, "invalid_gate 0 y gauss (10.0, 2.0)\n");
        fclose(f);
    }
    
    std::string output = run_nhssta("-l -d " + temp_dlib + " -b ../example/ex4.bench");
    
    // Should contain error (may be about unknown gate or parsing error)
    EXPECT_NE(output.find("error"), std::string::npos) << "Output: " << output;
    
    // Cleanup
    unlink(temp_dlib.c_str());
}

// Test: Exit code for errors
TEST_F(ErrorHandlingTest, ExitCodeOnError) {
    std::string cmd = nhssta_path + " -l -b ../example/ex4.bench 2>&1";
    int exit_code = system(cmd.c_str());
    
    // Should exit with non-zero code
    EXPECT_NE(exit_code, 0);
}

