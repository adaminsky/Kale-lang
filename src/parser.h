#ifndef PARSER_H
#define PARSER_H

#include <memory>
#include <map>
#include <cstdio>
#include <string>
#include "ast.h"

extern int CurTok;
extern int getNextToken();

/// BinopPrecedence - This holds the precedence for each binary operator that is
/// defined.
extern std::map<char, int> BinopPrecedence;

/// GetTokPrecedence - Get the precedence of the pending binary operator token.
extern int GetTokPrecedence();

extern std::unique_ptr<ExprAST> ParseExpression();

/// numberexpr ::= number
extern std::unique_ptr<ExprAST> ParseNumberExpr();

/// parenexpr ::= '(' expression ')'
extern std::unique_ptr<ExprAST> ParseParenExpr();

/// identifierexpr
///   ::= identifier
///   ::= identifier '(' expression* ')'
extern std::unique_ptr<ExprAST> ParseIdentifierExpr();

/// primary
///   ::= identifierexpr
///   ::= numberexpr
///   ::= parenexpr
extern std::unique_ptr<ExprAST> ParsePrimary();

/// binoprhs
///   ::= ('+' primary)*
extern std::unique_ptr<ExprAST> ParseBinOpRHS(int ExprPrec,
                                              std::unique_ptr<ExprAST> LHS);

/// expression
///   ::= primary binoprhs
///
extern std::unique_ptr<ExprAST> ParseExpression();

/// prototype
///   ::= id '(' id* ')'
extern std::unique_ptr<PrototypeAST> ParsePrototype();

/// definition ::= 'def' prototype expression
extern std::unique_ptr<FunctionAST> ParseDefinition();

/// toplevelexpr ::= expression
extern std::unique_ptr<FunctionAST> ParseTopLevelExpr();

/// external ::= 'extern' prototype
extern std::unique_ptr<PrototypeAST> ParseExtern();

#endif	// PARSER_H
