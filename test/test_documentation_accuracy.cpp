// -*- c++ -*-
// Unit tests for documentation accuracy
// Issue #57: ドキュメント: READMEとCONTRIBUTING.mdの最新化

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <regex>
#include <sstream>
#include <string>

class DocumentationAccuracyTest : public ::testing::Test {
   protected:
    void SetUp() override {
        project_root = std::filesystem::current_path();
        readme_path = project_root / "README.md";
        contributing_path = project_root / "CONTRIBUTING.md";
    }

    std::string readFile(const std::filesystem::path& path) {
        std::ifstream file(path);
        if (!file.is_open()) {
            return "";
        }
        std::ostringstream oss;
        oss << file.rdbuf();
        return oss.str();
    }

    std::filesystem::path project_root;
    std::filesystem::path readme_path;
    std::filesystem::path contributing_path;
};

// Test: README.md should exist
TEST_F(DocumentationAccuracyTest, ReadmeExists) {
    EXPECT_TRUE(std::filesystem::exists(readme_path)) << "README.md should exist";
}

// Test: CONTRIBUTING.md should exist
TEST_F(DocumentationAccuracyTest, ContributingExists) {
    EXPECT_TRUE(std::filesystem::exists(contributing_path)) << "CONTRIBUTING.md should exist";
}

// Test: README.md should mention snake_case file naming convention
TEST_F(DocumentationAccuracyTest, ReadmeMentionsSnakeCase) {
    std::string content = readFile(readme_path);

    // Should mention .cpp and .hpp extensions
    EXPECT_TRUE(content.find(".cpp") != std::string::npos)
        << "README.md should mention .cpp extension";
    EXPECT_TRUE(content.find(".hpp") != std::string::npos)
        << "README.md should mention .hpp extension";

    // Should NOT mention old .C or .h extensions (unless in historical context)
    // Allow mentions in comments or historical context, but not as current standard
    std::regex old_ext_regex(R"(\b\.C\b|\b\.h\b(?!pp))");
    std::smatch match;
    if (std::regex_search(content, match, old_ext_regex)) {
        // If found, check if it's in a comment or historical context
        size_t pos = match.position();
        std::string context = content.substr(std::max(0, (int)pos - 50), 100);
        // Allow if it's clearly historical or commented out
        if (context.find("old") == std::string::npos && context.find("以前") == std::string::npos &&
            context.find("//") == std::string::npos && context.find("#") == std::string::npos) {
            FAIL() << "README.md should not mention old .C or .h extensions as current standard";
        }
    }
}

// Test: CONTRIBUTING.md should mention snake_case file naming convention
TEST_F(DocumentationAccuracyTest, ContributingMentionsSnakeCase) {
    std::string content = readFile(contributing_path);

    // Should mention .cpp and .hpp extensions
    EXPECT_TRUE(content.find(".cpp") != std::string::npos ||
                content.find("snake_case") != std::string::npos)
        << "CONTRIBUTING.md should mention .cpp extension or snake_case naming";

    // Should NOT mention old .C extensions as current standard
    // Allow in comments or historical context
    if (content.find(".C") != std::string::npos) {
        size_t pos = content.find(".C");
        std::string context = content.substr(std::max(0, (int)pos - 50), 100);
        if (context.find("//") == std::string::npos && context.find("以前") == std::string::npos &&
            context.find("old") == std::string::npos) {
            FAIL() << "CONTRIBUTING.md should not mention .C extension as current standard (found "
                      "at position "
                   << pos << ")";
        }
    }
}

// Test: README.md should mention GTEST_DIR override mechanism
TEST_F(DocumentationAccuracyTest, ReadmeMentionsGtestDirOverride) {
    std::string content = readFile(readme_path);

    // Should mention GTEST_DIR or environment variable override
    EXPECT_TRUE(content.find("GTEST_DIR") != std::string::npos ||
                content.find("環境変数") != std::string::npos ||
                content.find("environment variable") != std::string::npos)
        << "README.md should mention GTEST_DIR override mechanism";
}

// Test: CONTRIBUTING.md should mention GTEST_DIR override mechanism
TEST_F(DocumentationAccuracyTest, ContributingMentionsGtestDirOverride) {
    std::string content = readFile(contributing_path);

    // Should mention GTEST_DIR or environment variable override
    EXPECT_TRUE(content.find("GTEST_DIR") != std::string::npos ||
                content.find("環境変数") != std::string::npos ||
                content.find("environment variable") != std::string::npos)
        << "CONTRIBUTING.md should mention GTEST_DIR override mechanism";
}

// Test: README.md should mention dev-setup and dev-check scripts
TEST_F(DocumentationAccuracyTest, ReadmeMentionsDevScripts) {
    std::string content = readFile(readme_path);

    // Should mention dev-setup or dev-check
    EXPECT_TRUE(content.find("dev-setup") != std::string::npos ||
                content.find("dev-check") != std::string::npos ||
                content.find("dev-setup.sh") != std::string::npos ||
                content.find("run-all-checks.sh") != std::string::npos)
        << "README.md should mention development setup scripts";
}

// Test: README.md test count should be updated (check for reasonable range)
// Note: This test checks that test count is mentioned and is reasonable
TEST_F(DocumentationAccuracyTest, ReadmeTestCountReasonable) {
    std::string content = readFile(readme_path);

    // Should mention test count
    EXPECT_TRUE(content.find("test") != std::string::npos ||
                content.find("テスト") != std::string::npos)
        << "README.md should mention tests";

    // If test count is mentioned, it should be >= 300 (current count is 316)
    std::regex test_count_regex(R"((\d+)\s+test)");
    std::smatch match;
    if (std::regex_search(content, match, test_count_regex)) {
        int count = std::stoi(match[1].str());
        EXPECT_GE(count, 300) << "Test count in README.md should be >= 300 (current: " << count
                              << ")";
    }
}

// Test: CONTRIBUTING.md should mention regression test framework
TEST_F(DocumentationAccuracyTest, ContributingMentionsRegressionTests) {
    std::string content = readFile(contributing_path);

    // Should mention regression test or test_regression
    EXPECT_TRUE(content.find("回帰テスト") != std::string::npos ||
                content.find("regression") != std::string::npos ||
                content.find("test_regression") != std::string::npos)
        << "CONTRIBUTING.md should mention regression test framework";
}

// Test: CONTRIBUTING.md should mention clang-format usage
TEST_F(DocumentationAccuracyTest, ContributingMentionsClangFormat) {
    std::string content = readFile(contributing_path);

    // Should mention clang-format
    EXPECT_TRUE(content.find("clang-format") != std::string::npos)
        << "CONTRIBUTING.md should mention clang-format";
}

// Test: CONTRIBUTING.md should mention clang-tidy usage
TEST_F(DocumentationAccuracyTest, ContributingMentionsClangTidy) {
    std::string content = readFile(contributing_path);

    // Should mention clang-tidy
    EXPECT_TRUE(content.find("clang-tidy") != std::string::npos)
        << "CONTRIBUTING.md should mention clang-tidy";
}

// Test: README.md should mention make check vs make dev-check distinction
TEST_F(DocumentationAccuracyTest, ReadmeMentionsCheckVsDevCheck) {
    std::string content = readFile(readme_path);

    // Should mention make check or make dev-check
    EXPECT_TRUE(content.find("make check") != std::string::npos ||
                content.find("make dev-check") != std::string::npos)
        << "README.md should mention make check or make dev-check";
}
