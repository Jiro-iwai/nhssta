// -*- c++ -*-
// Unit tests for development scripts functionality
// Issue #54: 改善: テスト実行のプリセットスクリプト追加

#include <gtest/gtest.h>
#include <cstdlib>
#include <string>
#include <fstream>
#include <sys/stat.h>

class DevScriptsTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Verify that scripts directory exists
        struct stat info;
        scripts_dir = "../scripts";
        if (stat(scripts_dir.c_str(), &info) != 0) {
            GTEST_SKIP() << "scripts directory not found";
        }
    }

    void TearDown() override {
        // Cleanup
    }

    bool file_exists(const std::string& path) {
        struct stat info;
        return stat(path.c_str(), &info) == 0;
    }

    bool is_executable(const std::string& path) {
        struct stat info;
        if (stat(path.c_str(), &info) != 0) {
            return false;
        }
        return (info.st_mode & S_IXUSR) != 0;
    }

    std::string scripts_dir;
};

// Test: Verify dev-setup.sh script exists
TEST_F(DevScriptsTest, DevSetupScriptExists) {
    std::string script_path = scripts_dir + "/dev-setup.sh";
    EXPECT_TRUE(file_exists(script_path)) << "dev-setup.sh should exist";
}

// Test: Verify dev-setup.sh is executable
TEST_F(DevScriptsTest, DevSetupScriptIsExecutable) {
    std::string script_path = scripts_dir + "/dev-setup.sh";
    if (file_exists(script_path)) {
        EXPECT_TRUE(is_executable(script_path)) << "dev-setup.sh should be executable";
    }
}

// Test: Verify run-all-checks.sh script exists
TEST_F(DevScriptsTest, RunAllChecksScriptExists) {
    std::string script_path = scripts_dir + "/run-all-checks.sh";
    EXPECT_TRUE(file_exists(script_path)) << "run-all-checks.sh should exist";
}

// Test: Verify run-all-checks.sh is executable
TEST_F(DevScriptsTest, RunAllChecksScriptIsExecutable) {
    std::string script_path = scripts_dir + "/run-all-checks.sh";
    if (file_exists(script_path)) {
        EXPECT_TRUE(is_executable(script_path)) << "run-all-checks.sh should be executable";
    }
}

// Test: Verify generate_coverage.sh script exists (existing script)
TEST_F(DevScriptsTest, GenerateCoverageScriptExists) {
    std::string script_path = scripts_dir + "/generate_coverage.sh";
    EXPECT_TRUE(file_exists(script_path)) << "generate_coverage.sh should exist";
}

// Test: Verify generate_coverage.sh is executable
TEST_F(DevScriptsTest, GenerateCoverageScriptIsExecutable) {
    std::string script_path = scripts_dir + "/generate_coverage.sh";
    if (file_exists(script_path)) {
        EXPECT_TRUE(is_executable(script_path)) << "generate_coverage.sh should be executable";
    }
}

// Test: Verify scripts directory structure
TEST_F(DevScriptsTest, ScriptsDirectoryStructure) {
    EXPECT_TRUE(file_exists(scripts_dir)) << "scripts directory should exist";
    
    // Check for expected scripts
    EXPECT_TRUE(file_exists(scripts_dir + "/dev-setup.sh"));
    EXPECT_TRUE(file_exists(scripts_dir + "/run-all-checks.sh"));
    EXPECT_TRUE(file_exists(scripts_dir + "/generate_coverage.sh"));
}

