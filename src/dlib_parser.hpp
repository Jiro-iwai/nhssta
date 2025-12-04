// -*- c++ -*-
// Authors: IWAI Jiro
// DlibParser: .dlib file parser for gate library

#ifndef NH_DLIB_PARSER_H
#define NH_DLIB_PARSER_H

#include <string>
#include <unordered_map>
#include <nhssta/gate.hpp>

namespace Nh {

// Forward declaration
class Parser;

class DlibParser {
public:
    using Gates = std::unordered_map<std::string, Gate>;
    
    explicit DlibParser(const std::string& dlib_file);
    void parse();
    const Gates& gates() const { return gates_; }
    
private:
    void parse_line(Parser& parser);
    
    std::string dlib_file_;
    Gates gates_;
};

}  // namespace Nh

#endif  // NH_DLIB_PARSER_H

