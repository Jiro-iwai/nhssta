// -*- c++ -*-
// Unit tests for Parser class
// Phase 2: Parser handling organization

#include <gtest/gtest.h>
#include "Parser.h"
#include <fstream>
#include <sstream>
#include <cstdio>
#include <sys/stat.h>

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
    
    EXPECT_THROW(parser.checkFile(), Parser::exception);
    try {
        parser.checkFile();
    } catch (Parser::exception& e) {
        EXPECT_NE(std::string(e.what()).find("failed to open file"), std::string::npos);
    }
}

// Test: Reading lines with comments
TEST_F(ParserTest, GetLineWithComments) {
    std::string content = "# comment line\n"
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
    std::string content = "\n\n"
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
    EXPECT_THROW(parser.getToken(value), Parser::exception);
    
    deleteTestFile("test7.txt");
}

// Test: Checking separator
TEST_F(ParserTest, CheckSeparatorSuccess) {
    std::string content = "(token)\n";
    std::string filepath = createTestFile("test8.txt", content);
    
    Parser parser(filepath, '#', "(),", " \t\r");
    parser.checkFile();
    parser.getLine();
    
    EXPECT_NO_THROW(parser.checkSepalator('('));
    
    std::string token;
    parser.getToken(token);
    EXPECT_EQ(token, "token");
    
    EXPECT_NO_THROW(parser.checkSepalator(')'));
    
    deleteTestFile("test8.txt");
}

// Test: Wrong separator (should throw)
TEST_F(ParserTest, CheckSeparatorFailure) {
    std::string content = "(token)\n";
    std::string filepath = createTestFile("test9.txt", content);
    
    Parser parser(filepath, '#', "(),", " \t\r");
    parser.checkFile();
    parser.getLine();
    
    EXPECT_THROW(parser.checkSepalator('['), Parser::exception);
    
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
    EXPECT_THROW(parser.checkEnd(), Parser::exception);
    
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
    EXPECT_THROW(parser.getToken(token2), Parser::exception);
    
    deleteTestFile("test12.txt");
}

// Test: Parsing dlib-like format
TEST_F(ParserTest, ParseDlibFormat) {
    std::string content = "# comment\n"
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
    parser.checkSepalator('(');
    parser.getToken(param1);
    parser.checkSepalator(',');
    parser.getToken(param2);
    parser.checkSepalator(')');
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
    parser.checkSepalator('(');
    parser.getToken(param1);
    parser.checkSepalator(',');
    parser.getToken(param2);
    parser.checkSepalator(')');
    parser.checkEnd();
    
    EXPECT_EQ(gate_name, "gate_name2");
    EXPECT_EQ(pin, 1);
    EXPECT_DOUBLE_EQ(param1, 20.0);
    EXPECT_DOUBLE_EQ(param2, 3.0);
    
    deleteTestFile("test_dlib.txt");
}

// Test: Parsing bench-like format
TEST_F(ParserTest, ParseBenchFormat) {
    std::string content = "INPUT(signal1)\n"
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
    parser.checkSepalator('(');
    parser.getToken(signal);
    EXPECT_EQ(signal, "signal1");
    parser.checkSepalator(')');
    parser.checkEnd();
    
    // Parse OUTPUT line
    parser.getLine();
    parser.getToken(keyword);
    EXPECT_EQ(keyword, "OUTPUT");
    parser.checkSepalator('(');
    parser.getToken(signal);
    EXPECT_EQ(signal, "signal2");
    parser.checkSepalator(')');
    parser.checkEnd();
    
    // Parse NET line
    parser.getLine();
    parser.getToken(keyword);  // N1
    parser.checkSepalator('=');
    parser.getToken(keyword);  // GATE
    parser.checkSepalator('(');
    parser.getToken(signal);   // signal1
    parser.checkSepalator(',');
    parser.getToken(signal);   // signal3
    parser.checkSepalator(')');
    parser.checkEnd();
    
    deleteTestFile("test_bench.txt");
}

// Test: File name and line number reporting
TEST_F(ParserTest, FileNameAndLineNumber) {
    std::string content = "line1\n"
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

