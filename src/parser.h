#ifndef PARSER_H
#define PARSER_H

#include <memory>
#include <map>
#include <cstdio>
#include <string>
#include "ast.h"
#include "lexer.h"

class Parser {
    public:
        int _curTok;
        int getNextToken();
        Lexer lex;

        /// BinopPrecedence - This holds the precedence for each binary operator that is
        /// defined.
//        std::map<char, int> _binopPrecedence;

        /// GetTokPrecedence - Get the precedence of the pending binary operator token.
        int GetTokPrecedence();

        /// numberexpr ::= number
        std::unique_ptr<ExprAST> ParseNumberExpr();

        /// parenexpr ::= '(' expression ')'
        std::unique_ptr<ExprAST> ParseParenExpr();

        /// identifierexpr
        ///   ::= identifier
        ///   ::= identifier '(' expression* ')'
        std::unique_ptr<ExprAST> ParseIdentifierExpr();

        /// primary
        ///   ::= identifierexpr
        ///   ::= numberexpr
        ///   ::= parenexpr
        std::unique_ptr<ExprAST> ParsePrimary();

        /// binoprhs
        ///   ::= ('+' primary)*
        std::unique_ptr<ExprAST> ParseBinOpRHS(int ExprPrec,
                std::unique_ptr<ExprAST> LHS);

        /// expression
        ///   ::= primary binoprhs
        ///
        std::unique_ptr<ExprAST> ParseExpression();

        /// prototype
        ///   ::= id '(' id* ')'
        std::unique_ptr<PrototypeAST> ParsePrototype();

        /// definition ::= 'def' prototype expression
        std::unique_ptr<FunctionAST> ParseDefinition();

        /// toplevelexpr ::= expression
        std::unique_ptr<FunctionAST> ParseTopLevelExpr();

        /// external ::= 'extern' prototype
        std::unique_ptr<PrototypeAST> ParseExtern();

        /// ifexpr ::= 'if' expression 'then' expression 'else' expression
        std::unique_ptr<ExprAST> ParseIfExpr();

        /// forexpr ::= 'for' identifier '=' expr ',' expr (',' expr)? 'in' expression
        std::unique_ptr<ExprAST> ParseForExpr();

        /// unary
        ///   ::= primary
        ///   ::= '!' unary
        std::unique_ptr<ExprAST> ParseUnary();

        /// varexpr ::= 'var' identifier ('=' expression)?
        ///                   (',' identifier ('=' expression)?)* 'in' expression
        std::unique_ptr<ExprAST> ParseVarExpr();
};

#endif	// PARSER_H
