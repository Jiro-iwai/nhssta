#include <gtest/gtest.h>
#include <sys/stat.h>

#include <cstdio>
#include <fstream>
#include <sstream>

#include "parser.hpp"

// Test boost::lexical_cast behavior before replacement
// This test verifies the current behavior of Parser::getToken with different types

class ParserLexicalCastTest : public ::testing::Test {
   protected:
    void SetUp() override {
        test_dir = "../test/test_data";
        // Create test directory if it doesn't exist
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

    std::string test_dir;
};

// Test: Parsing int values
TEST_F(ParserLexicalCastTest, ParseInt) {
    std::string content = "123 456 -789\n";
    std::string filepath = createTestFile("test_int.txt", content);

    Parser parser(filepath, '#', "(),", " \t\r");
    parser.checkFile();
    parser.getLine();

    int val1, val2, val3;
    parser.getToken(val1);
    parser.getToken(val2);
    parser.getToken(val3);

    EXPECT_EQ(val1, 123);
    EXPECT_EQ(val2, 456);
    EXPECT_EQ(val3, -789);

    deleteTestFile("test_int.txt");
}

// Test: Parsing double values
TEST_F(ParserLexicalCastTest, ParseDouble) {
    std::string content = "15.0 2.5 -3.14159 1e-5\n";
    std::string filepath = createTestFile("test_double.txt", content);

    Parser parser(filepath, '#', "(),", " \t\r");
    parser.checkFile();
    parser.getLine();

    double val1, val2, val3, val4;
    parser.getToken(val1);
    parser.getToken(val2);
    parser.getToken(val3);
    parser.getToken(val4);

    EXPECT_DOUBLE_EQ(val1, 15.0);
    EXPECT_DOUBLE_EQ(val2, 2.5);
    EXPECT_DOUBLE_EQ(val3, -3.14159);
    EXPECT_DOUBLE_EQ(val4, 1e-5);

    deleteTestFile("test_double.txt");
}

// Test: Parsing char values
TEST_F(ParserLexicalCastTest, ParseChar) {
    std::string content = "a b c\n";
    std::string filepath = createTestFile("test_char.txt", content);

    Parser parser(filepath, '#', "(),", " \t\r");
    parser.checkFile();
    parser.getLine();

    char val1, val2, val3;
    parser.getToken(val1);
    parser.getToken(val2);
    parser.getToken(val3);

    EXPECT_EQ(val1, 'a');
    EXPECT_EQ(val2, 'b');
    EXPECT_EQ(val3, 'c');

    deleteTestFile("test_char.txt");
}

// Test: Parsing string values
TEST_F(ParserLexicalCastTest, ParseString) {
    std::string content = "gate_name gauss const\n";
    std::string filepath = createTestFile("test_string.txt", content);

    Parser parser(filepath, '#', "(),", " \t\r");
    parser.checkFile();
    parser.getLine();

    std::string val1, val2, val3;
    parser.getToken(val1);
    parser.getToken(val2);
    parser.getToken(val3);

    EXPECT_EQ(val1, "gate_name");
    EXPECT_EQ(val2, "gauss");
    EXPECT_EQ(val3, "const");

    deleteTestFile("test_string.txt");
}

// Test: Invalid int conversion (should throw)
TEST_F(ParserLexicalCastTest, ParseInvalidInt) {
    std::string content = "abc\n";
    std::string filepath = createTestFile("test_invalid_int.txt", content);

    Parser parser(filepath, '#', "(),", " \t\r");
    parser.checkFile();
    parser.getLine();

    int val;
    EXPECT_THROW(parser.getToken(val), Nh::ParseException);

    deleteTestFile("test_invalid_int.txt");
}

// Test: Invalid double conversion (should throw)
TEST_F(ParserLexicalCastTest, ParseInvalidDouble) {
    std::string content = "not_a_number\n";
    std::string filepath = createTestFile("test_invalid_double.txt", content);

    Parser parser(filepath, '#', "(),", " \t\r");
    parser.checkFile();
    parser.getLine();

    double val;
    EXPECT_THROW(parser.getToken(val), Nh::ParseException);

    deleteTestFile("test_invalid_double.txt");
}

// Test: Mixed types (like dlib format)
TEST_F(ParserLexicalCastTest, ParseMixedTypes) {
    std::string content = "gate_name 0 y gauss (15.0, 2.0)\n";
    std::string filepath = createTestFile("test_mixed.txt", content);

    Parser parser(filepath, '#', "(),", " \t\r");
    parser.checkFile();
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

    EXPECT_EQ(gate_name, "gate_name");
    EXPECT_EQ(pin, 0);
    EXPECT_EQ(y, 'y');
    EXPECT_EQ(delay_type, "gauss");
    EXPECT_DOUBLE_EQ(param1, 15.0);
    EXPECT_DOUBLE_EQ(param2, 2.0);

    deleteTestFile("test_mixed.txt");
}

// Test: Empty string token (edge case)
TEST_F(ParserLexicalCastTest, ParseEmptyString) {
    // Empty string token should not occur with current tokenizer,
    // but we test the behavior
    std::string content = "token1  token2\n";  // Multiple spaces
    std::string filepath = createTestFile("test_empty.txt", content);

    Parser parser(filepath, '#', "(),", " \t\r");
    parser.checkFile();
    parser.getLine();

    std::string val1, val2;
    parser.getToken(val1);
    parser.getToken(val2);

    EXPECT_EQ(val1, "token1");
    EXPECT_EQ(val2, "token2");

    deleteTestFile("test_empty.txt");
}
