#include "llvm/IR/Verifier.h"

#include "ast.h"

std::unique_ptr<llvm::LLVMContext> TheContext;
std::unique_ptr<llvm::IRBuilder<>> Builder;
std::unique_ptr<llvm::Module> TheModule;
std::unique_ptr<llvm::legacy::FunctionPassManager> TheFPM;
std::map<std::string, llvm::AllocaInst *> NamedValues;
std::map<std::string, std::unique_ptr<PrototypeAST>> FunctionProtos;
std::map<char, int> BinopPrecedence;

// Create an alloca instruction in the entry block of the function. This is used
// for mutable variables etc.
llvm::AllocaInst *CreateEntryBlockAlloca(llvm::Function *TheFunction,
    const std::string &VarName) {
  llvm::IRBuilder<> TmpB(&TheFunction->getEntryBlock(),
      TheFunction->getEntryBlock().begin());
  return TmpB.CreateAlloca(llvm::Type::getDoubleTy(*TheContext), 0, VarName.c_str());
}

/// LogError* - These are little helper functions for error handling.
std::unique_ptr<ExprAST> LogError(const char *Str) {
  fprintf(stderr, "Error: %s\n", Str);
  return nullptr;
}
std::unique_ptr<PrototypeAST> LogErrorP(const char *Str) {
  LogError(Str);
  return nullptr;
}
llvm::Value *LogErrorV(const char *Str) {
  LogError(Str);
  return nullptr;
}



class codegenVisitor : public Visitor {
  llvm::Value* lastReturn;
  llvm::Function *getFunction(std::string Name) {
    // First, see if the function has already been added to the current module.
    if (auto *F = TheModule->getFunction(Name))
      return F;

    // If not, check whether we can codegen the declaration from some existing
    // prototype.
    auto FI = FunctionProtos.find(Name);
    if (FI != FunctionProtos.end()) {
      FI->second->accept(this);
      return generatedCode;
    }

    // If no existing prototype exists, return null.
    return nullptr;
  }

public:
  llvm::Function* generatedCode;
  void visit(NumberExprAST* e) {
    lastReturn = llvm::ConstantFP::get(*TheContext, llvm::APFloat(e->Val));
  }
  void visit(VariableExprAST* e) {
    llvm::Value *V = NamedValues[e->Name];
    if (!V) {
      lastReturn = LogErrorV("Unknown variable name");
      return;
    }

    // Load the value
    lastReturn = Builder->CreateLoad(V, e->Name.c_str());
  }
  void visit(BinaryExprAST* e) {
    // Special case '=' because we don't want to emit the LHS as an expression
    if (e->Op == '=') {
      VariableExprAST *LHSE = static_cast<VariableExprAST*>(e->LHS.get());
      if (!LHSE) {
        lastReturn = LogErrorV("destination of '=' must be a variable");
        return;
      }

      // Codegen the RHS
      e->RHS->accept(this);
      llvm::Value *Val = lastReturn;
      if (!Val) {
        lastReturn = nullptr;
        return;
      }

      llvm::Value *Variable = NamedValues[std::string(LHSE->getName())];
      if (!Variable) {
        lastReturn = LogErrorV("Unknown variable name");
        return;
      }
      Builder->CreateStore(Val, Variable);
      lastReturn = Val;
      return;
    }

    e->LHS->accept(this);
    llvm::Value *L = lastReturn;
    e->RHS->accept(this);
    llvm::Value *R = lastReturn;
    if (!L || !R) {
      lastReturn = nullptr;
      return;
    }

    switch (e->Op) {
      case '+':
        lastReturn = Builder->CreateFAdd(L, R, "addtmp");
        return;
      case '-':
        lastReturn = Builder->CreateFSub(L, R, "subtmp");
        return;
      case '*':
        lastReturn = Builder->CreateFMul(L, R, "multmp");
        return;
      case '<':
        L = Builder->CreateFCmpULT(L, R, "cmptmp");
        // Convert bool 0/1 to double 0.0 or 1.0
        lastReturn = Builder->CreateUIToFP(L, llvm::Type::getDoubleTy(*TheContext),
            "booltmp");
        return;
      default:
        break;
    }

    llvm::Function *F = getFunction(std::string("binary") + e->Op);
    assert(F && "binary operator not found!");

    llvm::Value *Ops[2] = {L, R};
    lastReturn = Builder->CreateCall(F, Ops, "binop");
  }
  void visit(CallExprAST* expr) {
    // Look up the name in the global module table.
    llvm::Function *CalleeF = getFunction(expr->Callee);
    if (!CalleeF) {
      lastReturn = LogErrorV("Unknown function referenced");
      return;
    }

    // If argument mismatch error.
    if (CalleeF->arg_size() != expr->Args.size()) {
      lastReturn = LogErrorV("Incorrect # arguments passed");
      return;
    }

    std::vector<llvm::Value *> ArgsV;
    for (unsigned i = 0, e = expr->Args.size(); i != e; ++i) {
      expr->Args[i]->accept(this);
      ArgsV.push_back(lastReturn);
      if (!ArgsV.back()) {
        lastReturn = nullptr;
        return;
      }
    }

    lastReturn = Builder->CreateCall(CalleeF, ArgsV, "calltmp");
  }
  void visit(PrototypeAST* e) {
    // Make the function type:  double(double,double) etc.
    std::vector<llvm::Type *> Doubles(e->Args.size(), llvm::Type::getDoubleTy(*TheContext));
    llvm::FunctionType *FT =
      llvm::FunctionType::get(llvm::Type::getDoubleTy(*TheContext), Doubles, false);

    llvm::Function *F =
      llvm::Function::Create(FT, llvm::Function::ExternalLinkage, e->Name, TheModule.get());

    // Set names for all arguments.
    unsigned Idx = 0;
    for (auto &Arg : F->args())
      Arg.setName(e->Args[Idx++]);

    generatedCode = F;
  }
  void visit(FunctionAST* e) {
    // Transfer ownership of the prototype to the FunctionProtos map, but keep a
    // reference to it for use below.
    auto & P = *e->Proto;
    FunctionProtos[e->Proto->getName()] = std::move(e->Proto);
    llvm::Function *TheFunction = getFunction(P.getName());
    if (!TheFunction) {
      generatedCode = nullptr;
      return;
    }

    // If this is an operator, install it
    if (P.isBinaryOp())
      BinopPrecedence[P.getOperatorName()] = P.getBinaryPrecedence();

    // Create a new basic block to start insertion into.
    llvm::BasicBlock *BB = llvm::BasicBlock::Create(*TheContext, "entry", TheFunction);
    Builder->SetInsertPoint(BB);

    // Record the function arguments in the NamedValues map.
    NamedValues.clear();
    for (auto &Arg : TheFunction->args()) {
      // Create an alloca
      llvm::AllocaInst *Alloca = CreateEntryBlockAlloca(TheFunction, std::string(Arg.getName()));
      Builder->CreateStore(&Arg, Alloca);
      NamedValues[std::string(Arg.getName())] = Alloca;
    }

    e->Body->accept(this);
    if (llvm::Value *RetVal = lastReturn) {
      // Finish off the function.
      Builder->CreateRet(RetVal);

      // Validate the generated code, checking for consistency.
      llvm::verifyFunction(*TheFunction);

      // Optimize the function.
      TheFPM->run(*TheFunction);

      generatedCode = TheFunction;
      return;
    }

    // Error reading body, remove function.
    TheFunction->eraseFromParent();

    if (P.isBinaryOp())
      BinopPrecedence.erase(P.getOperatorName());
    generatedCode = nullptr;
  }
  void visit(IfExprAST* e) {
    e->Cond->accept(this);
    llvm::Value *CondV = lastReturn;
    if (!CondV) {
      lastReturn = nullptr;
      return;
    }

    // Convert condition to a bool by comparing non-equal to 0.0
    CondV = Builder->CreateFCmpONE(
        CondV, llvm::ConstantFP::get(*TheContext, llvm::APFloat(0.0)), "ifcond");

    llvm::Function *TheFunction = Builder->GetInsertBlock()->getParent();

    // Create blocks for the then and else cases.  Insert the 'then' block at the
    // end of the function.
    llvm::BasicBlock *ThenBB =
      llvm::BasicBlock::Create(*TheContext, "then", TheFunction);
    llvm::BasicBlock *ElseBB = llvm::BasicBlock::Create(*TheContext, "else");
    llvm::BasicBlock *MergeBB = llvm::BasicBlock::Create(*TheContext, "ifcont");

    Builder->CreateCondBr(CondV, ThenBB, ElseBB);

    // Emit then value
    Builder->SetInsertPoint(ThenBB);

    e->Then->accept(this);
    llvm::Value *ThenV = lastReturn;
    if (!ThenV) {
      lastReturn = nullptr;
      return;
    }

    Builder->CreateBr(MergeBB);
    // Codegen of 'Then' can change the current block, update ThenBB for the PHI
    ThenBB = Builder->GetInsertBlock();

    // Emit else block
    TheFunction->getBasicBlockList().push_back(ElseBB);
    Builder->SetInsertPoint(ElseBB);

    e->Else->accept(this);
    llvm::Value *ElseV = lastReturn;
    if (!ElseV) {
      lastReturn = nullptr;
      return;
    }

    Builder->CreateBr(MergeBB);
    // codegen of 'Else' an change the current block, update ElseBB for the PHI
    ElseBB = Builder->GetInsertBlock();

    // Emit merge block
    TheFunction->getBasicBlockList().push_back(MergeBB);
    Builder->SetInsertPoint(MergeBB);
    llvm::PHINode *PN =
      Builder->CreatePHI(llvm::Type::getDoubleTy(*TheContext), 2, "iftmp");
    PN->addIncoming(ThenV, ThenBB);
    PN->addIncoming(ElseV, ElseBB);
    lastReturn = PN;
  }
  void visit(ForExprAST* e) {
    // Make the new basic block for the loop header, inserting after current
    // block
    llvm::Function *TheFunction = Builder->GetInsertBlock()->getParent();

    // Create an alloca for the variable in the entry block
    llvm::AllocaInst *Alloca = CreateEntryBlockAlloca(TheFunction, e->VarName);

    // Emit the start code first, without 'variable' in scope
    e->Start->accept(this);
    llvm::Value *StartVal = lastReturn;
    if (!StartVal) {
      lastReturn = nullptr;
      return;
    }

    // Store the value into the alloca
    Builder->CreateStore(StartVal, Alloca);

    llvm::BasicBlock *LoopBB =
      llvm::BasicBlock::Create(*TheContext, "loop", TheFunction);

    // Insert an explicit fall through from the current block to the LoopBB
    Builder->CreateBr(LoopBB);

    // Start insertion in LoopBB
    Builder->SetInsertPoint(LoopBB);

    // If the loop variable shadows an existing variable, we have to restore it.
    llvm::AllocaInst *OldVal = NamedValues[e->VarName];
    NamedValues[e->VarName] = Alloca;

    // Emit the body of the loop.  This, like any other expr, can change the
    // current BB.  Note that we ignore the value computed by the body, but don't
    // allow an error.
    e->Body->accept(this);
    if (!lastReturn) {
      lastReturn = nullptr;
      return;
    }

    // Emit the setp value
    llvm::Value *StepVal = nullptr;
    if (e->Step) {
      e->Step->accept(this);
      StepVal = lastReturn;
      if (!StepVal) {
        lastReturn = nullptr;
        return;
      }
    } else {
      // If not specified, use 1.0
      StepVal = llvm::ConstantFP::get(*TheContext, llvm::APFloat(1.0));
    }

    // Compute the end condition
    e->End->accept(this);
    llvm::Value *EndCond = lastReturn;
    if (!EndCond) {
      lastReturn = nullptr;
      return;
    }

    llvm::Value *CurVar = Builder->CreateLoad(Alloca, e->VarName.c_str());
    llvm::Value *NextVar = Builder->CreateFAdd(CurVar, StepVal, "nextvar");
    Builder->CreateStore(NextVar, Alloca);

    // Convert condition to a bool by comparing non-equal to 0.0
    EndCond = Builder->CreateFCmpONE(
        EndCond, llvm::ConstantFP::get(*TheContext, llvm::APFloat(0.0)), "loopcond");

    // Create the "after loop" block and insert it
    llvm::BasicBlock *AfterBB =
      llvm::BasicBlock::Create(*TheContext, "afterloop", TheFunction);

    // Insert the conditional branch into the end of LoopEndBB.
    Builder->CreateCondBr(EndCond, LoopBB, AfterBB);

    // Any new code will be inserted in AfterBB.
    Builder->SetInsertPoint(AfterBB);

    // Restore the unshadowed variable
    if (OldVal)
      NamedValues[e->VarName] = OldVal;
    else
      NamedValues.erase(e->VarName);

    // for expr always returns 0.0
    lastReturn = llvm::Constant::getNullValue(llvm::Type::getDoubleTy(*TheContext));
  }
  void visit(UnaryExprAST* e) {
    e->Operand->accept(this);
    llvm::Value *OperandV = lastReturn;
    if (!OperandV) {
      lastReturn = nullptr;
      return;
    }

    llvm::Function *F = getFunction(std::string("unary") + e->Opcode);
    if (!F) {
      lastReturn = LogErrorV("Unknown unary operator");
      return;
    }

    lastReturn = Builder->CreateCall(F, OperandV, "unop");
  }
  void visit(VarExprAST* expr) {
    std::vector<llvm::AllocaInst *> OldBindings;
    llvm::Function *TheFunction = Builder->GetInsertBlock()->getParent();

    // Register all variables and emit their initializer
    for (unsigned i = 0, e = expr->VarNames.size(); i != e; ++i) {
      const std::string &VarName = expr->VarNames[i].first;
      ExprAST *Init = expr->VarNames[i].second.get();

      // Emit the initializer before adding the variable to scope
      llvm::Value *InitVal;
      if (Init) {
        Init->accept(this);
        InitVal = lastReturn;
        if (!InitVal) {
          lastReturn = nullptr;
          return;
        }
      } else { // If not specified use 0.0
        InitVal = llvm::ConstantFP::get(*TheContext, llvm::APFloat(0.0));
      }

      llvm::AllocaInst *Alloca = CreateEntryBlockAlloca(TheFunction, VarName);
      Builder->CreateStore(InitVal, Alloca);

      // Remember the old variable binding to restore after the body
      OldBindings.push_back(NamedValues[VarName]);
      NamedValues[VarName] = Alloca;
    }

    // Codegen the body
    expr->Body->accept(this);
    llvm::Value *BodyVal = lastReturn;
    if (!BodyVal) {
      lastReturn = nullptr;
      return;
    }

    for (unsigned i = 0, e = expr->VarNames.size(); i != e; ++i)
      NamedValues[expr->VarNames[i].first] = OldBindings[i];
    lastReturn = BodyVal;
  }
};

bool PrototypeAST::isUnaryOp() const {
  return IsOperator && Args.size() == 1;
}

bool PrototypeAST::isBinaryOp() const {
  return IsOperator && Args.size() == 2;
}

char PrototypeAST::getOperatorName() const {
  assert(isUnaryOp() || isBinaryOp());
  return Name[Name.size() - 1];
}

unsigned PrototypeAST::getBinaryPrecedence() const {
  return Precedence;
}
