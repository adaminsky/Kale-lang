#ifndef LEXER_H
#define LEXER_H

// The lexer returns tokens [0-255] if it is an unknown character, otherwise one
// of these for known things.
enum Token {
  tok_eof = -1,

  // commands
  tok_def = -2,
  tok_extern = -3,

  // primary
  tok_identifier = -4,
  tok_number = -5,

  // control
  tok_if = -6,
  tok_then = -7,
  tok_else = -8,
  tok_for = -9,
  tok_in = -10,

  // operators
  tok_binary = -11,
  tok_unary = -12,

  // definition
  tok_var = -13
};

class Lexer {
    private:
        int _lastChar = ' ';
    public:
        std::string IdentifierStr; // Filled in if tok_identifier
        double NumVal;             // Filled in if tok_number
        int gettok();
};

#endif	// LEXER_H
