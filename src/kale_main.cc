#include "llvm/ADT/APFloat.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/Verifier.h"
#include "parser.h"
#include "lexer.h"
#include "ast.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <memory>
#include <string>
#include <vector>

//===----------------------------------------------------------------------===//
// Top-Level parsing
//===----------------------------------------------------------------------===//

static void HandleDefinition(Parser& parser) {
  if (auto FnAST = parser.ParseDefinition()) {
    if (auto *FnIR = FnAST->codegen()) {
      fprintf(stderr, "Parsed a function definition.\n");
      FnIR->print(llvm::errs());
      fprintf(stderr, "\n");
    }
  } else {
    // Skip token for error recovery.
    parser.getNextToken();
  }
}

static void HandleExtern(Parser& parser) {
  if (auto ProtoAST = parser.ParseExtern()) {
    if (auto *FnIR = ProtoAST->codegen()) {
      fprintf(stderr, "Parsed an extern\n");
      FnIR->print(llvm::errs());
      fprintf(stderr, "\n");
    }
  } else {
    // Skip token for error recovery.
    parser.getNextToken();
  }
}

static void HandleTopLevelExpression(Parser& parser) {
  // Evaluate a top-level expression into an anonymous function.
  if (auto FnAST = parser.ParseTopLevelExpr()) {
    if (auto *FnIR = FnAST->codegen()) {
      fprintf(stderr, "Parsed a top-level expr\n");
      FnIR->print(llvm::errs());
      fprintf(stderr, "\n");
    }
  } else {
    // Skip token for error recovery.
    parser.getNextToken();
  }
}

/// top ::= definition | external | expression | ';'
static void MainLoop(Parser& parser) {
  while (true) {
    fprintf(stderr, "ready> ");
    switch (parser._curTok) {
    case tok_eof:
      return;
    case ';': // ignore top-level semicolons.
      parser.getNextToken();
      break;
    case tok_def:
      HandleDefinition(parser);
      break;
    case tok_extern:
      HandleExtern(parser);
      break;
    default:
      HandleTopLevelExpression(parser);
      break;
    }
  }
}

//===----------------------------------------------------------------------===//
// Main driver code.
//===----------------------------------------------------------------------===//

int main() {
  Parser parser;
  // Install standard binary operators.
  // 1 is lowest precedence.
  parser._binopPrecedence['<'] = 10;
  parser._binopPrecedence['+'] = 20;
  parser._binopPrecedence['-'] = 20;
  parser._binopPrecedence['*'] = 40; // highest.

  // Prime the first token.
  fprintf(stderr, "ready> ");
  parser.getNextToken();

  TheModule = std::make_unique<llvm::Module>("my cool jit", TheContext);

  // Run the main "interpreter loop" now.
  MainLoop(parser);

  TheModule->print(llvm::errs(), nullptr);

  return 0;
}
