// -*- c++ -*-
// Unit tests for Parser class
// Phase 2: Parser handling organization

#include <gtest/gtest.h>
#include <sys/stat.h>

#include <cstdio>
#include <fstream>
#include <sstream>

#include "parser.hpp"

using Nh::Parser;

class ParserTest : public ::testing::Test {
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

// Test: Successful file opening
TEST_F(ParserTest, CheckFileSuccess) {
    std::string content = "test line\n";
    std::string filepath = createTestFile("test1.txt", content);

    Parser parser(filepath, '#', "(),", " \t\r");
    EXPECT_NO_THROW(parser.checkFile());

    deleteTestFile("test1.txt");
}

// Test: File not found
TEST_F(ParserTest, CheckFileNotFound) {
    std::string filepath = test_dir + "/nonexistent.txt";
    Parser parser(filepath, '#', "(),", " \t\r");

    // checkFile() now throws Nh::FileException
    EXPECT_THROW(parser.checkFile(), Nh::FileException);
    try {
        parser.checkFile();
    } catch (Nh::FileException& e) {
        EXPECT_NE(std::string(e.what()).find("File error"), std::string::npos);
        EXPECT_NE(std::string(e.what()).find("failed to open file"), std::string::npos);
    }
}

// Test: Reading lines with comments
TEST_F(ParserTest, GetLineWithComments) {
    std::string content =
        "# comment line\n"
        "actual line\n"
        "  # another comment\n"
        "another actual line\n";
    std::string filepath = createTestFile("test2.txt", content);

    Parser parser(filepath, '#', "(),", " \t\r");
    parser.checkFile();

    std::istream& line1 = parser.getLine();
    EXPECT_FALSE(line1.eof());

    std::istream& line2 = parser.getLine();
    EXPECT_FALSE(line2.eof());

    std::istream& line3 = parser.getLine();
    EXPECT_TRUE(line3.eof());

    deleteTestFile("test2.txt");
}

// Test: Reading empty lines (should skip)
TEST_F(ParserTest, GetLineEmptyLines) {
    std::string content =
        "\n\n"
        "actual line\n"
        "\n";
    std::string filepath = createTestFile("test3.txt", content);

    Parser parser(filepath, '#', "(),", " \t\r");
    parser.checkFile();

    std::istream& line = parser.getLine();
    EXPECT_FALSE(line.eof());

    deleteTestFile("test3.txt");
}

// Test: Getting string tokens
TEST_F(ParserTest, GetTokenString) {
    std::string content = "token1 token2 token3\n";
    std::string filepath = createTestFile("test4.txt", content);

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

    deleteTestFile("test4.txt");
}

// Test: Getting integer tokens
TEST_F(ParserTest, GetTokenInt) {
    std::string content = "123 456 789\n";
    std::string filepath = createTestFile("test5.txt", content);

    Parser parser(filepath, '#', "(),", " \t\r");
    parser.checkFile();
    parser.getLine();

    int val1, val2, val3;
    parser.getToken(val1);
    parser.getToken(val2);
    parser.getToken(val3);

    EXPECT_EQ(val1, 123);
    EXPECT_EQ(val2, 456);
    EXPECT_EQ(val3, 789);

    deleteTestFile("test5.txt");
}

// Test: Getting double tokens
TEST_F(ParserTest, GetTokenDouble) {
    std::string content = "12.5 34.56 78.9\n";
    std::string filepath = createTestFile("test6.txt", content);

    Parser parser(filepath, '#', "(),", " \t\r");
    parser.checkFile();
    parser.getLine();

    double val1, val2, val3;
    parser.getToken(val1);
    parser.getToken(val2);
    parser.getToken(val3);

    EXPECT_DOUBLE_EQ(val1, 12.5);
    EXPECT_DOUBLE_EQ(val2, 34.56);
    EXPECT_DOUBLE_EQ(val3, 78.9);

    deleteTestFile("test6.txt");
}

// Test: Invalid token conversion (should throw)
TEST_F(ParserTest, GetTokenInvalidConversion) {
    std::string content = "not_a_number\n";
    std::string filepath = createTestFile("test7.txt", content);

    Parser parser(filepath, '#', "(),", " \t\r");
    parser.checkFile();
    parser.getLine();

    int value;
    EXPECT_THROW(parser.getToken(value), Nh::ParseException);

    deleteTestFile("test7.txt");
}

// Test: Checking separator
TEST_F(ParserTest, CheckSeparatorSuccess) {
    std::string content = "(token)\n";
    std::string filepath = createTestFile("test8.txt", content);

    Parser parser(filepath, '#', "(),", " \t\r");
    parser.checkFile();
    parser.getLine();

    EXPECT_NO_THROW(parser.checkSeparator('('));

    std::string token;
    parser.getToken(token);
    EXPECT_EQ(token, "token");

    EXPECT_NO_THROW(parser.checkSeparator(')'));

    deleteTestFile("test8.txt");
}

// Test: Wrong separator (should throw)
TEST_F(ParserTest, CheckSeparatorFailure) {
    std::string content = "(token)\n";
    std::string filepath = createTestFile("test9.txt", content);

    Parser parser(filepath, '#', "(),", " \t\r");
    parser.checkFile();
    parser.getLine();

    EXPECT_THROW(parser.checkSeparator('['), Nh::ParseException);

    deleteTestFile("test9.txt");
}

// Test: Checking end of line
TEST_F(ParserTest, CheckEndSuccess) {
    std::string content = "single_token\n";
    std::string filepath = createTestFile("test10.txt", content);

    Parser parser(filepath, '#', "(),", " \t\r");
    parser.checkFile();
    parser.getLine();

    std::string token;
    parser.getToken(token);
    EXPECT_NO_THROW(parser.checkEnd());

    deleteTestFile("test10.txt");
}

// Test: Not at end of line (should throw)
TEST_F(ParserTest, CheckEndFailure) {
    std::string content = "token1 token2\n";
    std::string filepath = createTestFile("test11.txt", content);

    Parser parser(filepath, '#', "(),", " \t\r");
    parser.checkFile();
    parser.getLine();

    std::string token;
    parser.getToken(token);
    EXPECT_THROW(parser.checkEnd(), Nh::ParseException);

    deleteTestFile("test11.txt");
}

// Test: Unexpected termination
TEST_F(ParserTest, UnexpectedTermination) {
    std::string content = "token1\n";
    std::string filepath = createTestFile("test12.txt", content);

    Parser parser(filepath, '#', "(),", " \t\r");
    parser.checkFile();
    parser.getLine();

    std::string token1, token2;
    parser.getToken(token1);
    EXPECT_THROW(parser.getToken(token2), Nh::ParseException);

    deleteTestFile("test12.txt");
}

// Test: Parsing dlib-like format
TEST_F(ParserTest, ParseDlibFormat) {
    std::string content =
        "# comment\n"
        "gate_name 0 y gauss (15.0, 2.0)\n"
        "# another comment\n"
        "gate_name2 1 y gauss (20.0, 3.0)\n";
    std::string filepath = createTestFile("test_dlib.txt", content);

    Parser parser(filepath, '#', "(),", " \t\r");
    parser.checkFile();

    // Parse first gate line
    parser.getLine();
    std::string gate_name, delay_type;
    int pin;
    char y;
    double param1, param2;

    parser.getToken(gate_name);
    parser.getToken(pin);
    parser.getToken(y);
    parser.getToken(delay_type);
    parser.checkSeparator('(');
    parser.getToken(param1);
    parser.checkSeparator(',');
    parser.getToken(param2);
    parser.checkSeparator(')');
    parser.checkEnd();

    EXPECT_EQ(gate_name, "gate_name");
    EXPECT_EQ(pin, 0);
    EXPECT_EQ(y, 'y');
    EXPECT_EQ(delay_type, "gauss");
    EXPECT_DOUBLE_EQ(param1, 15.0);
    EXPECT_DOUBLE_EQ(param2, 2.0);

    // Parse second gate line
    parser.getLine();
    parser.getToken(gate_name);
    parser.getToken(pin);
    parser.getToken(y);
    parser.getToken(delay_type);
    parser.checkSeparator('(');
    parser.getToken(param1);
    parser.checkSeparator(',');
    parser.getToken(param2);
    parser.checkSeparator(')');
    parser.checkEnd();

    EXPECT_EQ(gate_name, "gate_name2");
    EXPECT_EQ(pin, 1);
    EXPECT_DOUBLE_EQ(param1, 20.0);
    EXPECT_DOUBLE_EQ(param2, 3.0);

    deleteTestFile("test_dlib.txt");
}

// Test: Parsing bench-like format
TEST_F(ParserTest, ParseBenchFormat) {
    std::string content =
        "INPUT(signal1)\n"
        "OUTPUT(signal2)\n"
        "N1 = GATE(signal1, signal3)\n";
    std::string filepath = createTestFile("test_bench.txt", content);

    Parser parser(filepath, '#', "(),=", " \t\r");
    parser.checkFile();

    // Parse INPUT line
    parser.getLine();
    std::string keyword, signal;
    parser.getToken(keyword);
    EXPECT_EQ(keyword, "INPUT");
    parser.checkSeparator('(');
    parser.getToken(signal);
    EXPECT_EQ(signal, "signal1");
    parser.checkSeparator(')');
    parser.checkEnd();

    // Parse OUTPUT line
    parser.getLine();
    parser.getToken(keyword);
    EXPECT_EQ(keyword, "OUTPUT");
    parser.checkSeparator('(');
    parser.getToken(signal);
    EXPECT_EQ(signal, "signal2");
    parser.checkSeparator(')');
    parser.checkEnd();

    // Parse NET line
    parser.getLine();
    parser.getToken(keyword);  // N1
    parser.checkSeparator('=');
    parser.getToken(keyword);  // GATE
    parser.checkSeparator('(');
    parser.getToken(signal);  // signal1
    parser.checkSeparator(',');
    parser.getToken(signal);  // signal3
    parser.checkSeparator(')');
    parser.checkEnd();

    deleteTestFile("test_bench.txt");
}

// Test: File name and line number reporting
TEST_F(ParserTest, FileNameAndLineNumber) {
    std::string content =
        "line1\n"
        "line2\n"
        "line3\n";
    std::string filepath = createTestFile("test13.txt", content);

    Parser parser(filepath, '#', "(),", " \t\r");
    parser.checkFile();

    EXPECT_EQ(parser.getFileName(), filepath);
    EXPECT_EQ(parser.getNumLine(), 0);

    parser.getLine();
    EXPECT_EQ(parser.getNumLine(), 1);

    parser.getLine();
    EXPECT_EQ(parser.getNumLine(), 2);

    deleteTestFile("test13.txt");
}

// Test: Many consecutive comment lines (regression test for #137)
// This test verifies that the parser can handle many consecutive comment lines
// without stack overflow. The original implementation used recursion which
// could cause stack overflow with many comment lines.
TEST_F(ParserTest, ManyConsecutiveCommentLines) {
    // Generate content with many consecutive comment lines
    // Using 1000 lines which is enough to verify the fix without being too slow
    std::string content;
    for (int i = 0; i < 1000; i++) {
        content += "# comment line " + std::to_string(i) + "\n";
    }
    content += "actual_data value\n";
    content += "# trailing comment\n";

    std::string filepath = createTestFile("test_many_comments.txt", content);
    // Use standard dlib-style separators (no keep separator for this test)
    Parser parser(filepath, '#', "", " \t\r");
    parser.checkFile();

    // getLine() should skip all comment lines and return the actual data line
    EXPECT_TRUE(parser.getLine().good());

    std::string token;
    parser.getToken(token);
    EXPECT_EQ(token, "actual_data");

    parser.getToken(token);
    EXPECT_EQ(token, "value");

    // Line number should be 1001 (1000 comment lines + 1 data line)
    EXPECT_EQ(parser.getNumLine(), 1001);

    deleteTestFile("test_many_comments.txt");
}

// Test: File with only comment lines
TEST_F(ParserTest, OnlyCommentLines) {
    std::string content;
    for (int i = 0; i < 100; i++) {
        content += "# comment line " + std::to_string(i) + "\n";
    }

    std::string filepath = createTestFile("test_only_comments.txt", content);
    Parser parser(filepath, '#', "", " \t\r");
    parser.checkFile();

    // getLine() should return EOF when all lines are comments
    std::istream& result = parser.getLine();
    EXPECT_TRUE(result.eof());

    deleteTestFile("test_only_comments.txt");
}

// Test: Empty lines mixed with comment lines
TEST_F(ParserTest, EmptyAndCommentLines) {
    std::string content =
        "# comment 1\n"
        "\n"
        "# comment 2\n"
        "\n"
        "\n"
        "data1 data2\n"
        "\n"
        "# comment 3\n";

    std::string filepath = createTestFile("test_empty_comments.txt", content);
    Parser parser(filepath, '#', "", " \t\r");
    parser.checkFile();

    // Should skip comments and empty lines
    EXPECT_TRUE(parser.getLine().good());

    std::string token;
    parser.getToken(token);
    EXPECT_EQ(token, "data1");

    // Line number should be 6 (the line with actual data)
    EXPECT_EQ(parser.getNumLine(), 6);

    deleteTestFile("test_empty_comments.txt");
}

// Test: File ending with comment lines (PR #142 review feedback)
// This ensures files ending with comments/empty lines are handled correctly
// without causing ParseException in callers like Ssta::read_dlib()
TEST_F(ParserTest, FileEndingWithComments) {
    std::string content =
        "data1 value1\n"
        "data2 value2\n"
        "# trailing comment 1\n"
        "# trailing comment 2\n";

    std::string filepath = createTestFile("test_trailing_comments.txt", content);
    Parser parser(filepath, '#', "", " \t\r");
    parser.checkFile();

    // First data line
    EXPECT_TRUE(parser.getLine().good());
    std::string token;
    parser.getToken(token);
    EXPECT_EQ(token, "data1");

    // Second data line
    EXPECT_TRUE(parser.getLine().good());
    parser.getToken(token);
    EXPECT_EQ(token, "data2");

    // Third call should return EOF (trailing comments are skipped)
    std::istream& result = parser.getLine();
    EXPECT_TRUE(result.eof() || result.fail());

    deleteTestFile("test_trailing_comments.txt");
}

// Test: File with only trailing content being comments
TEST_F(ParserTest, DataFollowedByManyComments) {
    std::string content = "single_data\n";
    for (int i = 0; i < 100; i++) {
        content += "# comment " + std::to_string(i) + "\n";
    }

    std::string filepath = createTestFile("test_data_then_comments.txt", content);
    Parser parser(filepath, '#', "", " \t\r");
    parser.checkFile();

    // Should get the single data line
    EXPECT_TRUE(parser.getLine().good());
    std::string token;
    parser.getToken(token);
    EXPECT_EQ(token, "single_data");

    // Next call should return EOF (all trailing comments skipped)
    std::istream& result = parser.getLine();
    EXPECT_TRUE(result.eof() || result.fail());

    deleteTestFile("test_data_then_comments.txt");
}
