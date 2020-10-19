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
#include "llvm/Transforms/Utils.h"
#include "llvm/Transforms/Scalar/GVN.h"
#include "llvm/Transforms/Utils/Mem2Reg.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/Host.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/TargetRegistry.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Target/TargetMachine.h"
#include "llvm/Target/TargetOptions.h"
#include "parser.h"
#include "lexer.h"
#include "ast.h"
#include "codegenVisitor.cc"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <memory>
#include <string>
#include <vector>
#include <system_error>
#include <utility>


//===----------------------------------------------------------------------===//
// Setup code for pass manager and JIT.
//===----------------------------------------------------------------------===//

void InitializeModuleAndPassManager(std::unique_ptr<llvm::orc::KaleidoscopeJIT>& TheJIT) {
  // Open a new module.
  TheModule = std::make_unique<llvm::Module>("my cool jit", TheContext);
  TheModule->setDataLayout(TheJIT->getTargetMachine().createDataLayout());

  // Create a new pass manager attached to it.
  TheFPM = std::make_unique<llvm::legacy::FunctionPassManager>(TheModule.get());

  // Promote allocas to registers
  TheFPM->add(llvm::createPromoteMemoryToRegisterPass());
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
    codegenVisitor* codeV = new codegenVisitor();
    FnAST->accept(codeV);
    if (llvm::Function *FnIR = codeV->generatedCode) {
      fprintf(stderr, "Parsed a function definition.\n");
      FnIR->print(llvm::errs());
      fprintf(stderr, "\n");
    }
    delete codeV;
  } else {
    // Skip token for error recovery.
    parser.getNextToken();
  }
}

static void HandleExtern(Parser& parser) {
  if (auto ProtoAST = parser.ParseExtern()) {
    codegenVisitor* codeV = new codegenVisitor();
    ProtoAST->accept(codeV);
    if (llvm::Function *FnIR = codeV->generatedCode) {
      fprintf(stderr, "Parsed an extern\n");
      FnIR->print(llvm::errs());
      fprintf(stderr, "\n");
      FunctionProtos[ProtoAST->getName()] = std::move(ProtoAST);
    }
    delete codeV;
  } else {
    // Skip token for error recovery.
    parser.getNextToken();
  }
}

static void HandleTopLevelExpression(Parser& parser, std::unique_ptr<llvm::orc::KaleidoscopeJIT>& TheJIT) {
  // Evaluate a top-level expression into an anonymous function.
  if (auto FnAST = parser.ParseTopLevelExpr()) {
    //FnAST->codegen();
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
  BinopPrecedence['='] = 2;
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

  llvm::InitializeAllTargetInfos();
  llvm::InitializeAllTargets();
  llvm::InitializeAllTargetMCs();
  llvm::InitializeAllAsmParsers();
  llvm::InitializeAllAsmPrinters();

  auto TargetTriple = llvm::sys::getDefaultTargetTriple();
  TheModule->setTargetTriple(TargetTriple);

  std::string Error;
  auto Target = llvm::TargetRegistry::lookupTarget(TargetTriple, Error);
  if (!Target) {
    llvm::errs() << Error;
    return 1;
  }

  auto CPU = "generic";
  auto Features = "";
  llvm::TargetOptions opt;
  auto RM = llvm::Optional<llvm::Reloc::Model>();
  auto TheTargetMachine =
    Target->createTargetMachine(TargetTriple, CPU, Features, opt, RM);
  TheModule->setDataLayout(TheTargetMachine->createDataLayout());

  auto Filename = "output.o";
  std::error_code EC;
  llvm::raw_fd_ostream dest(Filename, EC, llvm::sys::fs::OF_None);
  if (EC) {
    llvm::errs() << "Could not open file:" << EC.message();
    return 1;
  }

  llvm::legacy::PassManager pass;
#if LLVM_VERSION_MAJOR >= 10
  auto FileType = llvm::CGFT_ObjectFile;
#else
  auto FileType = llvm::LLVMTargetMachine::CGFT_ObjectFile;
#endif
  if (TheTargetMachine->addPassesToEmitFile(pass, dest, nullptr, FileType)) {
    llvm::errs() << "TheTargetMachine can't emit a file of this type";
    return 1;
  }

  pass.run(*TheModule);
  dest.flush();
  llvm::outs() << "Wrote " << Filename << "\n";

  return 0;
}
