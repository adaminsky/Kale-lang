#include <string>
#include <cstdlib>
#include <cctype>
#include "lexer.h"

/// gettok - Return the next token from standard input.
int Lexer::gettok() {
    // Skip any whitespace.
    while (isspace(_lastChar))
        _lastChar = getchar();

    if (isalpha(_lastChar)) { // identifier: [a-zA-Z][a-zA-Z0-9]*
        IdentifierStr = _lastChar;
        while (isalnum((_lastChar = getchar())))
            IdentifierStr += _lastChar;

        if (IdentifierStr == "def")
            return tok_def;
        if (IdentifierStr == "extern")
            return tok_extern;
        if (IdentifierStr == "if")
            return tok_if;
        if (IdentifierStr == "then")
            return tok_then;
        if (IdentifierStr == "else")
            return tok_else;
        if (IdentifierStr == "for")
            return tok_for;
        if (IdentifierStr == "in")
            return tok_in;
        if (IdentifierStr == "binary")
            return tok_binary;
        if (IdentifierStr == "unary")
            return tok_unary;
        if (IdentifierStr == "in")
            return tok_in;
        if (IdentifierStr == "var")
            return tok_var;
        return tok_identifier;
    }

    if (isdigit(_lastChar) || _lastChar == '.') { // Number: [0-9.]+
        std::string NumStr;
        do {
            NumStr += _lastChar;
            _lastChar = getchar();
        } while (isdigit(_lastChar) || _lastChar == '.');

        NumVal = strtod(NumStr.c_str(), nullptr);
        return tok_number;
    }

    if (_lastChar == '#') {
        // Comment until end of line.
        do
            _lastChar = getchar();
        while (_lastChar != EOF && _lastChar != '\n' && _lastChar != '\r');

        if (_lastChar != EOF)
            return gettok();
    }

    // Check for end of file.  Don't eat the EOF.
    if (_lastChar == EOF)
        return tok_eof;

    // Otherwise, just return the character as its ascii value.
    int ThisChar = _lastChar;
    _lastChar = getchar();
    return ThisChar;
}
