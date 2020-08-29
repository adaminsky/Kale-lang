#include "../include/KaleidoscopeJIT.h"
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
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/Transforms/InstCombine/InstCombine.h"
#include "llvm/Transforms/Scalar.h"
#include "llvm/Transforms/Scalar/GVN.h"
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
// Setup code for pass manager and JIT.
//===----------------------------------------------------------------------===//

void InitializeModuleAndPassManager(std::unique_ptr<llvm::orc::KaleidoscopeJIT>& TheJIT) {
  // Open a new module.
  TheModule = std::make_unique<llvm::Module>("my cool jit", TheContext);
  TheModule->setDataLayout(TheJIT->getTargetMachine().createDataLayout());

  // Create a new pass manager attached to it.
  TheFPM = std::make_unique<llvm::legacy::FunctionPassManager>(TheModule.get());

  // Do simple "peephole" optimizations and bit-twiddling optzns.
  TheFPM->add(llvm::createInstructionCombiningPass());
  // Reassociate expressions.
  TheFPM->add(llvm::createReassociatePass());
  // Eliminate Common SubExpressions.
  TheFPM->add(llvm::createGVNPass());
  // Simplify the control flow graph (deleting unreachable blocks, etc).
  TheFPM->add(llvm::createCFGSimplificationPass());

  TheFPM->doInitialization();
}

//===----------------------------------------------------------------------===//
// Top-Level parsing
//===----------------------------------------------------------------------===//

static void HandleDefinition(Parser& parser, std::unique_ptr<llvm::orc::KaleidoscopeJIT>& TheJIT) {
  if (auto FnAST = parser.ParseDefinition()) {
    if (auto *FnIR = FnAST->codegen()) {
      fprintf(stderr, "Parsed a function definition.\n");
      FnIR->print(llvm::errs());
      fprintf(stderr, "\n");
      TheJIT->addModule(std::move(TheModule));
      InitializeModuleAndPassManager(TheJIT);
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
      FunctionProtos[ProtoAST->getName()] = std::move(ProtoAST);
    }
  } else {
    // Skip token for error recovery.
    parser.getNextToken();
  }
}

static void HandleTopLevelExpression(Parser& parser, std::unique_ptr<llvm::orc::KaleidoscopeJIT>& TheJIT) {
  // Evaluate a top-level expression into an anonymous function.
  if (auto FnAST = parser.ParseTopLevelExpr()) {
    if (auto *FnIR = FnAST->codegen()) {
      // JIT the module containing the anonymous expression, keeping a handle so
      // we can free it later.
      auto H = TheJIT->addModule(std::move(TheModule));
      InitializeModuleAndPassManager(TheJIT);

      // Search the JIT for the __anon_expr symbol.
      auto ExprSymbol = TheJIT->findSymbol("__anon_expr");
      assert(ExprSymbol && "Function not found");

      // Get the symbol's address and cast it to the right type (takes no
      // arguments, returns a double) so we can call it as a native function.
      double (*FP)() = (double (*)())(intptr_t)cantFail(ExprSymbol.getAddress());
      fprintf(stderr, "Evaluated to %f\n", FP());

      // Delete the anonymous expression module from the JIT.
      TheJIT->removeModule(H);
    }
  } else {
    // Skip token for error recovery.
    parser.getNextToken();
  }
}

/// top ::= definition | external | expression | ';'
static void MainLoop(std::unique_ptr<llvm::orc::KaleidoscopeJIT> TheJIT, Parser& parser) {
  while (true) {
    fprintf(stderr, "ready> ");
    switch (parser._curTok) {
    case tok_eof:
      return;
    case ';': // ignore top-level semicolons.
      parser.getNextToken();
      break;
    case tok_def:
      HandleDefinition(parser, TheJIT);
      break;
    case tok_extern:
      HandleExtern(parser);
      break;
    default:
      HandleTopLevelExpression(parser, TheJIT);
      break;
    }
  }
}

//===----------------------------------------------------------------------===//
// "Library" functions that can be "extern'd" from user code.
//===----------------------------------------------------------------------===//

#ifdef _WIN32
#define DLLEXPORT __declspec(dllexport)
#else
#define DLLEXPORT
#endif

/// putchard - putchar that takes a double and returns 0.
extern "C" DLLEXPORT double putchard(double X) {
  fputc((char)X, stderr);
  return 0;
}

/// printd - printf that takes a double prints it as "%f\n", returning 0.
extern "C" DLLEXPORT double printd(double X) {
  fprintf(stderr, "%f\n", X);
  return 0;
}

//===----------------------------------------------------------------------===//
// Main driver code.
//===----------------------------------------------------------------------===//

int main() {
  std::unique_ptr<llvm::orc::KaleidoscopeJIT> TheJIT;

  LLVMInitializeNativeTarget();
  LLVMInitializeNativeAsmPrinter();
  LLVMInitializeNativeAsmParser();

  TheJIT = std::make_unique<llvm::orc::KaleidoscopeJIT>();

  Parser parser;
  // Install standard binary operators.
  // 1 is lowest precedence.
  BinopPrecedence['<'] = 10;
  BinopPrecedence['+'] = 20;
  BinopPrecedence['-'] = 20;
  BinopPrecedence['*'] = 40; // highest.

  // Prime the first token.
  fprintf(stderr, "ready> ");
  parser.getNextToken();

  InitializeModuleAndPassManager(TheJIT);

  // Run the main "interpreter loop" now.
  MainLoop(std::move(TheJIT), parser);

  TheModule->print(llvm::errs(), nullptr);

  return 0;
}
