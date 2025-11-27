#include <gtest/gtest.h>
#include <sys/stat.h>

#include <cstdio>
#include <fstream>
#include <sstream>
#include <vector>

#include "parser.hpp"

using Nh::Parser;

// Test boost::tokenizer behavior before replacement
// This test verifies the current tokenization behavior with different separators

class ParserTokenizerTest : public ::testing::Test {
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

    // Helper to collect all tokens from a line
    std::vector<std::string> collectTokens(Parser& parser) {
        std::vector<std::string> tokens;
        parser.getLine();
        while (true) {
            try {
                std::string token;
                parser.getToken(token);
                tokens.push_back(token);
            } catch (Nh::ParseException& e) {
                break;
            }
        }
        return tokens;
    }

    std::string test_dir;
};

// Test: Drop separators (spaces, tabs) are removed
TEST_F(ParserTokenizerTest, DropSeparatorsRemoved) {
    std::string content = "token1 token2  token3\n";
    std::string filepath = createTestFile("test_drop.txt", content);

    Parser parser(filepath, '#', "(),", " \t\r");
    parser.checkFile();
    parser.getLine();

    std::vector<std::string> tokens;
    std::string token;
    parser.getToken(token);
    tokens.push_back(token);
    parser.getToken(token);
    tokens.push_back(token);
    parser.getToken(token);
    tokens.push_back(token);

    EXPECT_EQ(tokens.size(), 3);
    EXPECT_EQ(tokens[0], "token1");
    EXPECT_EQ(tokens[1], "token2");
    EXPECT_EQ(tokens[2], "token3");

    deleteTestFile("test_drop.txt");
}

// Test: Keep separators (parentheses, commas) are preserved as tokens
TEST_F(ParserTokenizerTest, KeepSeparatorsPreserved) {
    std::string content = "gate_name(15.0, 2.0)\n";
    std::string filepath = createTestFile("test_keep.txt", content);

    Parser parser(filepath, '#', "(),", " \t\r");
    parser.checkFile();
    parser.getLine();

    std::vector<std::string> tokens;
    std::string token;
    while (true) {
        try {
            parser.getToken(token);
            tokens.push_back(token);
        } catch (Nh::ParseException& e) {
            break;
        }
    }

    // Should have: gate_name, (, 15.0, ,, 2.0, )
    EXPECT_GE(tokens.size(), 6);
    EXPECT_EQ(tokens[0], "gate_name");
    EXPECT_EQ(tokens[1], "(");
    EXPECT_EQ(tokens[2], "15.0");
    EXPECT_EQ(tokens[3], ",");
    EXPECT_EQ(tokens[4], "2.0");
    EXPECT_EQ(tokens[5], ")");

    deleteTestFile("test_keep.txt");
}

// Test: Mixed drop and keep separators (dlib format)
TEST_F(ParserTokenizerTest, MixedSeparatorsDlibFormat) {
    std::string content = "gate_name 0 y gauss (15.0, 2.0)\n";
    std::string filepath = createTestFile("test_mixed_dlib.txt", content);

    Parser parser(filepath, '#', "(),", " \t\r");
    parser.checkFile();
    parser.getLine();

    std::string gate_name, in, out, type;
    parser.getToken(gate_name);
    parser.getToken(in);
    parser.getToken(out);
    parser.getToken(type);
    parser.checkSeparator('(');
    double param1, param2;
    parser.getToken(param1);
    parser.checkSeparator(',');
    parser.getToken(param2);
    parser.checkSeparator(')');

    EXPECT_EQ(gate_name, "gate_name");
    EXPECT_EQ(in, "0");
    EXPECT_EQ(out, "y");
    EXPECT_EQ(type, "gauss");
    EXPECT_DOUBLE_EQ(param1, 15.0);
    EXPECT_DOUBLE_EQ(param2, 2.0);

    deleteTestFile("test_mixed_dlib.txt");
}

// Test: Mixed drop and keep separators (bench format with =)
TEST_F(ParserTokenizerTest, MixedSeparatorsBenchFormat) {
    std::string content = "NET signal1 = gate1(signal2, signal3)\n";
    std::string filepath = createTestFile("test_mixed_bench.txt", content);

    Parser parser(filepath, '#', "(),=", " \t\r");
    parser.checkFile();
    parser.getLine();

    std::vector<std::string> tokens;
    std::string token;
    while (true) {
        try {
            parser.getToken(token);
            tokens.push_back(token);
        } catch (Nh::ParseException& e) {
            break;
        }
    }

    // Should have: NET, signal1, =, gate1, (, signal2, ,, signal3, )
    EXPECT_GE(tokens.size(), 9);
    EXPECT_EQ(tokens[0], "NET");
    EXPECT_EQ(tokens[1], "signal1");
    EXPECT_EQ(tokens[2], "=");
    EXPECT_EQ(tokens[3], "gate1");
    EXPECT_EQ(tokens[4], "(");

    deleteTestFile("test_mixed_bench.txt");
}

// Test: Empty tokens between consecutive separators
TEST_F(ParserTokenizerTest, EmptyTokensBetweenSeparators) {
    std::string content = "gate_name(,)\n";  // Empty tokens between ( and ,
    std::string filepath = createTestFile("test_empty.txt", content);

    Parser parser(filepath, '#', "(),", " \t\r");
    parser.checkFile();
    parser.getLine();

    std::string gate_name;
    parser.getToken(gate_name);
    parser.checkSeparator('(');
    parser.checkSeparator(',');
    parser.checkSeparator(')');

    EXPECT_EQ(gate_name, "gate_name");

    deleteTestFile("test_empty.txt");
}

// Test: Comment lines are skipped
TEST_F(ParserTokenizerTest, CommentLinesSkipped) {
    std::string content =
        "# comment line\n"
        "actual_token\n"
        "# another comment\n";
    std::string filepath = createTestFile("test_comment.txt", content);

    Parser parser(filepath, '#', "(),", " \t\r");
    parser.checkFile();
    parser.getLine();

    std::string token;
    parser.getToken(token);
    EXPECT_EQ(token, "actual_token");

    deleteTestFile("test_comment.txt");
}

// Test: Multiple consecutive drop separators
TEST_F(ParserTokenizerTest, MultipleDropSeparators) {
    std::string content = "token1    token2\t\t\ttoken3\n";
    std::string filepath = createTestFile("test_multiple_drop.txt", content);

    Parser parser(filepath, '#', "(),", " \t\r");
    parser.checkFile();
    parser.getLine();

    std::string token1, token2, token3;
    parser.getToken(token1);
    parser.getToken(token2);
    parser.getToken(token3);

    EXPECT_EQ(token1, "token1");
    EXPECT_EQ(token2, "token2");
    EXPECT_EQ(token3, "token3");

    deleteTestFile("test_multiple_drop.txt");
}

// Test: Separators at start/end of line
TEST_F(ParserTokenizerTest, SeparatorsAtStartEnd) {
    std::string content = "(token)\n";
    std::string filepath = createTestFile("test_start_end.txt", content);

    Parser parser(filepath, '#', "(),", " \t\r");
    parser.checkFile();
    parser.getLine();

    parser.checkSeparator('(');
    std::string token;
    parser.getToken(token);
    parser.checkSeparator(')');

    EXPECT_EQ(token, "token");

    deleteTestFile("test_start_end.txt");
}
