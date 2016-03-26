// -*- c++ -*-
// Authors: IWAI Jiro
// $Id: Parser.h,v 1.1.1.1 2006/06/10 17:59:11 jiro Exp $

#ifndef PARSER__H
#define PARSER__H

#include <string>
#include <fstream>
#include <boost/tokenizer.hpp>
#include <boost/lexical_cast.hpp>

class Parser {

private:

    typedef boost::char_separator<char> Separator;
    typedef boost::tokenizer<Separator> Tokenizer;
    typedef Tokenizer::iterator Token;

public:

    class exception {
    public:
	exception(const std::string& what): what_(what) {}
	const std::string& what() { return what_; }
    private:
	std::string what_ ;
    };

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
	    u = boost::lexical_cast<U>(*token_);
	} catch ( boost::bad_lexical_cast& e ){
	    unexpectedToken_(*token_);
	}
	pre_ = *token_;
	token_++;
    }

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
    Separator sepalator_;
    const char begin_comment_;
    Tokenizer* tokenizer_;
    std::string pre_;
    Token token_;
};

#endif	// PARSER__H
