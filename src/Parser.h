// -*- c++ -*-
// Authors: IWAI Jiro

#ifndef PARSER__H
#define PARSER__H

#include <string>
#include <fstream>
#include <stdexcept>
#include <type_traits>
#include "Tokenizer.h"
#include "Exception.h"

class Parser {

private:

    typedef Tokenizer::iterator Token;

public:

    // Backward compatibility: keep exception as alias to Nh::ParseException
    // This will be removed in a later phase
    using exception = Nh::ParseException;

    Parser
    (
		const std::string& file,
		const char begin_comment,
		const char* keep_separator,
		const char* drop_separator = " \t"
		);

    ~Parser() { delete tokenizer_; }

    void checkFile();

    std::istream& getLine();

    template < class U >
    void getToken( U& u )
		{
			checkTermination();
			try {
				u = convertToken<U>(*token_);
			} catch ( std::exception& e ){
				unexpectedToken_(*token_);
			}
			pre_ = *token_;
			token_++;
		}

private:
    // Helper template for converting tokens to different types
    template <typename T>
    T convertToken(const std::string& token) {
        if constexpr (std::is_same_v<T, std::string>) {
            return token;
        } else if constexpr (std::is_same_v<T, int>) {
            return std::stoi(token);
        } else if constexpr (std::is_same_v<T, double>) {
            return std::stod(token);
        } else if constexpr (std::is_same_v<T, char>) {
            if (token.length() != 1) {
                throw std::invalid_argument("char token must be single character");
            }
            return token[0];
        } else {
            // For other types, try to use string conversion
            // This maintains compatibility with boost::lexical_cast behavior
            static_assert(std::is_convertible_v<std::string, T>,
                        "Type must be convertible from string");
            return static_cast<T>(token);
        }
    }

public:

    void checkSepalator( char sepalator );
    void checkEnd();
    void unexpectedToken();
    const std::string& getFileName() const { return file_; }
    int getNumLine() const { return line_number_; }

private:

    void unexpectedToken_(const std::string& token);
    bool fail() const { return infile_.fail(); }
    void checkTermination();
    Token& begin() { token_ = tokenizer_->begin(); return token_; }
    Token end() { return tokenizer_->end(); }

    int line_number_;
    std::string file_;
    std::string line_;
    std::ifstream infile_;
    std::string drop_separator_;
    std::string keep_separator_;
    const char begin_comment_;
    Tokenizer* tokenizer_;
    std::string pre_;
    Token token_;
};

#endif	// PARSER__H
