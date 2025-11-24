// -*- c++ -*-
// Unit tests for architecture documentation accuracy
// Issue #58: ドキュメント: アーキテクチャ概要ドキュメントの作成

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <regex>
#include <sstream>
#include <string>

class ArchitectureDocumentationTest : public ::testing::Test {
   protected:
    void SetUp() override {
        project_root = std::filesystem::current_path();
        docs_dir = project_root / "docs";
        architecture_path = docs_dir / "architecture.md";
        domain_model_path = docs_dir / "domain_model.md";
        data_flow_path = docs_dir / "data_flow.md";
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
    std::filesystem::path docs_dir;
    std::filesystem::path architecture_path;
    std::filesystem::path domain_model_path;
    std::filesystem::path data_flow_path;
};

// Test: docs directory should exist
TEST_F(ArchitectureDocumentationTest, DocsDirectoryExists) {
    EXPECT_TRUE(std::filesystem::exists(docs_dir)) << "docs/ directory should exist";
}

// Test: architecture.md should exist
TEST_F(ArchitectureDocumentationTest, ArchitectureDocExists) {
    EXPECT_TRUE(std::filesystem::exists(architecture_path)) << "docs/architecture.md should exist";
}

// Test: domain_model.md should exist
TEST_F(ArchitectureDocumentationTest, DomainModelDocExists) {
    EXPECT_TRUE(std::filesystem::exists(domain_model_path)) << "docs/domain_model.md should exist";
}

// Test: data_flow.md should exist
TEST_F(ArchitectureDocumentationTest, DataFlowDocExists) {
    EXPECT_TRUE(std::filesystem::exists(data_flow_path)) << "docs/data_flow.md should exist";
}

// Test: architecture.md should mention core domain models
TEST_F(ArchitectureDocumentationTest, ArchitectureMentionsCoreModels) {
    std::string content = readFile(architecture_path);

    // Should mention key components
    EXPECT_TRUE(content.find("RandomVariable") != std::string::npos ||
                content.find("random") != std::string::npos)
        << "architecture.md should mention RandomVariable";

    EXPECT_TRUE(content.find("Gate") != std::string::npos ||
                content.find("gate") != std::string::npos)
        << "architecture.md should mention Gate";

    EXPECT_TRUE(content.find("Ssta") != std::string::npos ||
                content.find("SSTA") != std::string::npos)
        << "architecture.md should mention Ssta";
}

// Test: domain_model.md should describe relationships
TEST_F(ArchitectureDocumentationTest, DomainModelDescribesRelationships) {
    std::string content = readFile(domain_model_path);

    // Should mention relationships between components
    EXPECT_TRUE(content.find("RandomVariable") != std::string::npos ||
                content.find("Gate") != std::string::npos ||
                content.find("Ssta") != std::string::npos ||
                content.find("Parser") != std::string::npos ||
                content.find("Covariance") != std::string::npos)
        << "domain_model.md should describe domain models and their relationships";
}

// Test: data_flow.md should describe the processing flow
TEST_F(ArchitectureDocumentationTest, DataFlowDescribesProcessingFlow) {
    std::string content = readFile(data_flow_path);

    // Should mention input files and processing steps
    EXPECT_TRUE(
        content.find("dlib") != std::string::npos || content.find("bench") != std::string::npos ||
        content.find("Parser") != std::string::npos || content.find("Ssta") != std::string::npos)
        << "data_flow.md should describe the data processing flow";
}

// Test: architecture.md should mention exception design policy
TEST_F(ArchitectureDocumentationTest, ArchitectureMentionsExceptionPolicy) {
    std::string content = readFile(architecture_path);

    // Should mention exception handling
    EXPECT_TRUE(content.find("exception") != std::string::npos ||
                content.find("Exception") != std::string::npos ||
                content.find("例外") != std::string::npos ||
                content.find("error") != std::string::npos ||
                content.find("Error") != std::string::npos)
        << "architecture.md should mention exception design policy";
}

// Test: Documentation should be readable (non-empty and reasonable length)
TEST_F(ArchitectureDocumentationTest, DocumentationIsReadable) {
    std::string arch_content = readFile(architecture_path);
    std::string domain_content = readFile(domain_model_path);
    std::string flow_content = readFile(data_flow_path);

    // Each document should have reasonable content
    EXPECT_GT(arch_content.length(), 100)
        << "architecture.md should have substantial content (at least 100 characters)";
    EXPECT_GT(domain_content.length(), 100)
        << "domain_model.md should have substantial content (at least 100 characters)";
    EXPECT_GT(flow_content.length(), 100)
        << "data_flow.md should have substantial content (at least 100 characters)";
}

// Test: Documentation should reference actual source files
TEST_F(ArchitectureDocumentationTest, DocumentationReferencesSourceFiles) {
    std::string arch_content = readFile(architecture_path);
    std::string domain_content = readFile(domain_model_path);
    std::string flow_content = readFile(data_flow_path);

    std::string combined = arch_content + domain_content + flow_content;

    // Should mention actual source files or directories
    EXPECT_TRUE(combined.find("src/") != std::string::npos ||
                combined.find(".cpp") != std::string::npos ||
                combined.find(".hpp") != std::string::npos ||
                combined.find("include/") != std::string::npos)
        << "Documentation should reference actual source files or directories";
}
