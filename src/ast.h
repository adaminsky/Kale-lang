#ifndef AST_H
#define AST_H

#include <string>
#include <memory>
#include <vector>
#include "llvm/IR/Value.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LegacyPassManager.h"

extern llvm::LLVMContext TheContext;
extern llvm::IRBuilder<> Builder;
extern std::unique_ptr<llvm::Module> TheModule;
extern std::unique_ptr<llvm::legacy::FunctionPassManager> TheFPM;
extern std::map<std::string, llvm::AllocaInst *> NamedValues;
extern std::map<char, int> BinopPrecedence;

class NumberExprAST;
class VariableExprAST;
class BinaryExprAST;
class CallExprAST;
class PrototypeAST;
class FunctionAST;
class IfExprAST;
class ForExprAST;
class UnaryExprAST;
class VarExprAST;

class Visitor {
public:
  virtual void visit(NumberExprAST* e) = 0;
  virtual void visit(VariableExprAST* e) = 0;
  virtual void visit(BinaryExprAST* e) = 0;
  virtual void visit(CallExprAST* e) = 0;
  virtual void visit(PrototypeAST* e) = 0;
  virtual void visit(FunctionAST* e) = 0;
  virtual void visit(IfExprAST* e) = 0;
  virtual void visit(ForExprAST* e) = 0;
  virtual void visit(UnaryExprAST* e) = 0;
  virtual void visit(VarExprAST* e) = 0;
};

/// ExprAST - Base class for all expression nodes.
class ExprAST {
public:
  virtual ~ExprAST() = default;
  virtual void accept(Visitor *v) = 0;
};

/// NumberExprAST - Expression class for numeric literals like "1.0".
class NumberExprAST : public ExprAST {
public:
  double Val;
  NumberExprAST(double Val) : Val(Val) {}
  void accept(Visitor* v) {
    v->visit(this);
  }
};

/// VariableExprAST - Expression class for referencing a variable, like "a".
class VariableExprAST : public ExprAST {
public:
  std::string Name;
  VariableExprAST(const std::string &Name) : Name(Name) {}
  void accept(Visitor* v) {
    v->visit(this);
  }
  const std::string &getName() const { return Name; }
};

/// BinaryExprAST - Expression class for a binary operator.
class BinaryExprAST : public ExprAST {
public:
  char Op;
  std::unique_ptr<ExprAST> LHS, RHS;
  BinaryExprAST(char Op, std::unique_ptr<ExprAST> LHS,
                std::unique_ptr<ExprAST> RHS)
      : Op(Op), LHS(std::move(LHS)), RHS(std::move(RHS)) {}
  void accept(Visitor* v) {
    v->visit(this);
  }
};

/// CallExprAST - Expression class for function calls.
class CallExprAST : public ExprAST {
public:
  std::string Callee;
  std::vector<std::unique_ptr<ExprAST>> Args;
  CallExprAST(const std::string &Callee,
              std::vector<std::unique_ptr<ExprAST>> Args)
      : Callee(Callee), Args(std::move(Args)) {}
  void accept(Visitor* v) {
    v->visit(this);
  }
};

/// PrototypeAST - This class represents the "prototype" for a function,
/// which captures its name, and its argument names (thus implicitly the number
/// of arguments the function takes).
class PrototypeAST {
public:
  std::string Name;
  std::vector<std::string> Args;
  bool IsOperator;
  unsigned Precedence;  // Precedence if a binary op
  PrototypeAST(const std::string &Name, std::vector<std::string> Args,
               bool IsOperator = false, unsigned Prec = 0)
      : Name(Name), Args(std::move(Args)), IsOperator(IsOperator),
        Precedence(Prec) {}

  void accept(Visitor* v) {
    v->visit(this);
  }
  const std::string &getName() const { return Name; }

  bool isUnaryOp() const;
  bool isBinaryOp() const;
  char getOperatorName() const;
  unsigned getBinaryPrecedence() const;
};

/// FunctionAST - This class represents a function definition itself.
class FunctionAST {
public:
  std::unique_ptr<PrototypeAST> Proto;
  std::unique_ptr<ExprAST> Body;
  FunctionAST(std::unique_ptr<PrototypeAST> Proto,
              std::unique_ptr<ExprAST> Body)
      : Proto(std::move(Proto)), Body(std::move(Body)) {}
  void accept(Visitor* v) {
    v->visit(this);
  }
};

// IfExprAST - Expression class for if/then/else
class IfExprAST : public ExprAST {
public:
    std::unique_ptr<ExprAST> Cond, Then, Else;
    IfExprAST(std::unique_ptr<ExprAST> Cond, std::unique_ptr<ExprAST> Then,
            std::unique_ptr<ExprAST> Else)
        : Cond(std::move(Cond)), Then(std::move(Then)), Else(std::move(Else)) {}

    void accept(Visitor* v) {
      v->visit(this);
    }
};

class ForExprAST : public ExprAST {
public:
    std::string VarName;
    std::unique_ptr<ExprAST> Start, End, Step, Body;
    ForExprAST(const std::string &VarName, std::unique_ptr<ExprAST> Start,
               std::unique_ptr<ExprAST> End, std::unique_ptr<ExprAST> Step,
               std::unique_ptr<ExprAST> Body)
        : VarName(VarName), Start(std::move(Start)), End(std::move(End)),
          Step(std::move(Step)), Body(std::move(Body)) {}

    void accept(Visitor* v) {
      v->visit(this);
    }
};

// UnaryExprAST - Expression class for a unary operator
class UnaryExprAST : public ExprAST {
public:
    char Opcode;
    std::unique_ptr<ExprAST> Operand;
    UnaryExprAST(char Opcode, std::unique_ptr<ExprAST> Operand)
        : Opcode(Opcode), Operand(std::move(Operand)) {}

    void accept(Visitor* v) {
      v->visit(this);
    }
};

// VarExprAST - Expression class for var/in
class VarExprAST : public ExprAST {
public:
    std::vector<std::pair<std::string, std::unique_ptr<ExprAST>>> VarNames;
    std::unique_ptr<ExprAST> Body;
    VarExprAST(std::vector<std::pair<std::string, std::unique_ptr<ExprAST>>> VarNames,
        std::unique_ptr<ExprAST> Body)
      : VarNames(std::move(VarNames)), Body(std::move(Body)) {}
    void accept(Visitor* v) {
      v->visit(this);
    }
};


extern std::unique_ptr<ExprAST> LogError(const char *Str);
extern std::unique_ptr<PrototypeAST> LogErrorP(const char *Str);
extern llvm::Value *LogErrorV(const char *Str);
extern std::map<std::string, std::unique_ptr<PrototypeAST>> FunctionProtos;
#endif	// AST_H
