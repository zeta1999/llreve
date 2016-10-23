#include "FixedAbstraction.h"

#include "Compat.h"
#include "Helper.h"
#include "Invariant.h"
#include "MarkAnalysis.h"
#include "Opts.h"

#include <set>

using std::make_shared;
using std::make_unique;
using smt::makeOp;
using smt::FunDef;
using smt::Op;
using smt::ConstantBool;
using smt::memoryVariable;
using smt::SharedSMTRef;
using smt::SMTRef;
using smt::SortedVar;
using smt::stringExpr;
using std::set;
using std::string;
using std::vector;

std::set<uint32_t> getVarArgs(const llvm::Function &fun) {
    std::set<uint32_t> varArgs;
    for (auto User : fun.users()) {
        if (const auto callInst = llvm::dyn_cast<llvm::CallInst>(User)) {
            varArgs.insert(callInst->getNumArgOperands() -
                           fun.getFunctionType()->getNumParams());
        } else {
            logWarningData("Unsupported use of function\n", *User);
        }
    }
    return varArgs;
}

static std::vector<SortedVar> externDeclArgs(const llvm::Function &fun1,
                                             const llvm::Function &fun2,
                                             unsigned numberOfArguments) {
    std::vector<SortedVar> args;
    auto funArgs1 = functionArgs(fun1);
    for (auto arg : funArgs1) {
        args.push_back(arg);
    }
    if (SMTGenerationOpts::getInstance().Heap) {
        args.push_back(SortedVar("HEAP$1", memoryType()));
    }
    auto funArgs2 = functionArgs(fun2);
    for (auto arg : funArgs2) {
        args.push_back(arg);
    }
    if (SMTGenerationOpts::getInstance().Heap) {
        args.push_back(SortedVar("HEAP$2", memoryType()));
    }
    args.push_back(SortedVar("res1", int64Type()));
    args.push_back(SortedVar("res2", int64Type()));
    if (SMTGenerationOpts::getInstance().Heap) {
        args.push_back(SortedVar("HEAP$1_res", memoryType()));
        args.push_back(SortedVar("HEAP$2_res", memoryType()));
    }
    return args;
}

void externDeclarations(const llvm::Module &mod1, const llvm::Module &mod2,
                        std::vector<SharedSMTRef> &declarations,
                        std::multimap<string, string> funCondMap) {
    for (const auto &functionPair :
         SMTGenerationOpts::getInstance().CoupledFunctions) {
        if (hasMutualFixedAbstraction(functionPair)) {
            if (SMTGenerationOpts::getInstance().DisableAutoAbstraction) {
                const auto assumeEquivalent =
                    SMTGenerationOpts::getInstance().AssumeEquivalent;
                if (assumeEquivalent.find(functionPair) !=
                    assumeEquivalent.end()) {
                    auto decls = equivalentExternDecls(
                        *functionPair.first, *functionPair.second, funCondMap);
                    declarations.insert(declarations.end(), decls.begin(),
                                        decls.end());
                } else {
                    auto decls = notEquivalentExternDecls(*functionPair.first,
                                                          *functionPair.second);
                    declarations.insert(declarations.end(), decls.begin(),
                                        decls.end());
                }
            } else {
                auto decls = equivalentExternDecls(
                    *functionPair.first, *functionPair.second, funCondMap);
                declarations.insert(declarations.end(), decls.begin(),
                                    decls.end());
            }
        }
    }
    for (auto &fun1 : mod1) {
        if (hasFixedAbstraction(fun1) && !isLlreveIntrinsic(fun1)) {
            auto decls = externFunDecl(fun1, Program::First);
            declarations.insert(declarations.end(), decls.begin(), decls.end());
        }
    }
    for (auto &fun2 : mod2) {
        if (hasFixedAbstraction(fun2) && !isLlreveIntrinsic(fun2)) {
            auto decls = externFunDecl(fun2, Program::Second);
            declarations.insert(declarations.end(), decls.begin(), decls.end());
        }
    }
}

static SMTRef equalOutputs(std::string functionName,
                           std::multimap<string, string> funCondMap) {
    SMTRef body = makeOp("=", "res1", "res2");
    if (SMTGenerationOpts::getInstance().Heap) {
        SharedSMTRef heapOutEqual = makeOp("=", memoryVariable("HEAP$1_res"),
                                           memoryVariable("HEAP$2_res"));
        body = makeOp("and", std::move(body), heapOutEqual);
    }

    std::vector<SharedSMTRef> equalOut;
    // TODO remove dependency on a single name
    auto range = funCondMap.equal_range(functionName);
    for (auto i = range.first; i != range.second; ++i) {
        equalOut.push_back(stringExpr(i->second));
    }
    if (!equalOut.empty()) {
        equalOut.push_back(std::move(body));
        body = make_unique<Op>("and", equalOut);
    }
    return body;
}

static SMTRef equalInputs(const llvm::Function &fun1,
                          const llvm::Function &fun2,
                          unsigned numberOfArguments) {
    std::vector<SharedSMTRef> equal;
    auto funArgs1 = functionArgs(fun1);
    auto funArgs2 = functionArgs(fun2);

    for (auto argPair : makeZip(funArgs1, funArgs2)) {
        equal.push_back(makeOp(
            "=", make_unique<smt::TypedVariable>(argPair.first.name,
                                                 argPair.first.type->copy()),
            make_unique<smt::TypedVariable>(argPair.second.name,
                                            argPair.second.type->copy())));
    }
    if (SMTGenerationOpts::getInstance().Heap) {
        SharedSMTRef heapInEqual =
            makeOp("=", memoryVariable("HEAP$1"), memoryVariable("HEAP$2"));
        equal.push_back(heapInEqual);
    }
    return make_unique<Op>("and", equal);
}

std::vector<SharedSMTRef>
equivalentExternDecls(const llvm::Function &fun1, const llvm::Function &fun2,
                      std::multimap<string, string> funCondMap) {
    vector<SharedSMTRef> declarations;
    set<uint32_t> varArgs = getVarArgs(fun1);
    set<uint32_t> varArgs2 = getVarArgs(fun2);
    for (auto el : varArgs2) {
        varArgs.insert(el);
    }
    for (const auto argNum : varArgs) {
        vector<SortedVar> args = externDeclArgs(fun1, fun2, argNum);
        std::string funName =
            invariantName(ENTRY_MARK, ProgramSelection::Both,
                          fun1.getName().str() + "^" + fun2.getName().str(),
                          InvariantAttr::NONE, argNum);

        SMTRef eqOutputs = equalOutputs(fun1.getName(), funCondMap);
        SMTRef eqInputs = equalInputs(fun1, fun2, argNum);
        SMTRef body = makeOp("=>", std::move(eqInputs), std::move(eqOutputs));

        SharedSMTRef mainInv =
            make_shared<FunDef>(funName, args, boolType(), std::move(body));
        declarations.push_back(std::move(mainInv));
    }
    return declarations;
}

std::vector<SharedSMTRef> notEquivalentExternDecls(const llvm::Function &fun1,
                                                   const llvm::Function &fun2) {
    vector<SharedSMTRef> declarations;
    set<uint32_t> varArgs = getVarArgs(fun1);
    set<uint32_t> varArgs2 = getVarArgs(fun2);
    for (auto el : varArgs2) {
        varArgs.insert(el);
    }
    for (const auto argNum : varArgs) {
        vector<SortedVar> args = externDeclArgs(fun1, fun2, argNum);
        std::string funName =
            invariantName(ENTRY_MARK, ProgramSelection::Both,
                          fun1.getName().str() + "^" + fun2.getName().str(),
                          InvariantAttr::NONE, argNum);
        SharedSMTRef mainInv = make_shared<FunDef>(
            funName, args, boolType(), make_unique<ConstantBool>(true));
        declarations.push_back(std::move(mainInv));
    }
    return declarations;
}

std::vector<SharedSMTRef> externFunDecl(const llvm::Function &fun,
                                        Program program) {
    std::vector<SharedSMTRef> decls;
    set<uint32_t> varArgs = getVarArgs(fun);
    for (auto argNum : varArgs) {
        std::vector<SortedVar> args = functionArgs(fun);
        if (SMTGenerationOpts::getInstance().Heap) {
            args.push_back(SortedVar("HEAP", memoryType()));
        }
        args.push_back(SortedVar("res", int64Type()));
        if (SMTGenerationOpts::getInstance().Heap) {
            args.push_back(SortedVar("HEAP_res", memoryType()));
        }
        std::string funName =
            invariantName(ENTRY_MARK, asSelection(program), fun.getName().str(),
                          InvariantAttr::NONE, argNum);
        SharedSMTRef body = make_unique<ConstantBool>(true);
        decls.push_back(make_shared<FunDef>(funName, args, boolType(), body));
    }
    return decls;
}