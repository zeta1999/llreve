/*
 * This file is part of
 *    llreve - Automatic regression verification for LLVM programs
 *
 * Copyright (C) 2016 Karlsruhe Institute of Technology
 *
 * The system is published under a BSD license.
 * See LICENSE (distributed with this file) for details.
 */

#include "llreve/dynamic/Interpreter.h"

#include "Compat.h"
#include "Helper.h"

#include "llvm/IR/Constants.h"

using llvm::Argument;
using llvm::BasicBlock;
using llvm::BinaryOperator;
using llvm::BranchInst;
using llvm::CastInst;
using llvm::CmpInst;
using llvm::ConstantInt;
using llvm::Function;
using llvm::GetElementPtrInst;
using llvm::ICmpInst;
using llvm::Instruction;
using llvm::LoadInst;
using llvm::PHINode;
using llvm::ReturnInst;
using llvm::SelectInst;
using llvm::StoreInst;
using llvm::SwitchInst;
using llvm::TerminatorInst;
using llvm::Value;
using llvm::dyn_cast;
using llvm::isa;

using std::function;
using std::make_shared;
using std::map;
using std::shared_ptr;
using std::static_pointer_cast;
using std::string;
using std::vector;

using nlohmann::json;

using namespace llreve::opts;

namespace llreve {
namespace dynamic {

unsigned HeapElemSizeFlag;
static llreve::cl::opt<unsigned, true> // The parser
    HeapElemSize("heap-elem-size",
                 llreve::cl::desc("Size for a random heap element"),
                 llreve::cl::location(HeapElemSizeFlag), llreve::cl::init(8));

json toJSON(const Integer &v) { return v.get_str(); }
bool unsafeBool(const Integer &val) {
    switch (val.type) {
    case IntType::Unbounded:
        logError("Called unsafeBool on an unbounded int\n");
        exit(1);
    case IntType::Bounded:
        if (val.bounded.getBitWidth() == 1) {
            return val.bounded.getBoolValue();
        } else {
            logError("Called unsafeBool on a bounded int of width > 1\n");
            exit(1);
        }
    }
}

bool isContainedIn(const llvm::SmallDenseMap<HeapAddress, Integer> &small,
                   const Heap &big) {
    for (const auto &val : small) {
        auto it = big.assignedValues.find(val.first);
        if (it != big.assignedValues.end()) {
            if (val.second != it->second) {
                return false;
            }
        } else {
            if (val.second != big.background) {
                return false;
            }
        }
    }
    return true;
}

bool operator==(const Heap &lhs, const Heap &rhs) {
    if (lhs.background != rhs.background) {
        return false;
    }
    return isContainedIn(lhs.assignedValues, rhs) &&
           isContainedIn(rhs.assignedValues, lhs);
}

MonoPair<FastCall>
interpretFunctionPair(MonoPair<const Function *> funs,
                      MonoPair<FastVarMap> variables, MonoPair<Heap> heaps,
                      uint32_t maxSteps,
                      const AnalysisResultsMap &analysisResults) {
    return makeMonoPair(
        interpretFunction(*funs.first, FastState(variables.first, heaps.first),
                          maxSteps, analysisResults),
        interpretFunction(*funs.second,
                          FastState(variables.second, heaps.second), maxSteps,
                          analysisResults));
}

MonoPair<FastCall> interpretFunctionPair(
    MonoPair<const llvm::Function *> funs, MonoPair<FastVarMap> variables,
    MonoPair<Heap> heaps, MonoPair<const llvm::BasicBlock *> startBlocks,
    uint32_t maxSteps, const AnalysisResultsMap &analysisResults) {
    return makeMonoPair(
        interpretFunction(*funs.first, FastState(variables.first, heaps.first),
                          startBlocks.first, maxSteps, analysisResults),
        interpretFunction(*funs.second,
                          FastState(variables.second, heaps.second),
                          startBlocks.second, maxSteps, analysisResults));
}

FastCall interpretFunction(const Function &fun, FastState entry,
                           const llvm::BasicBlock *startBlock,
                           uint32_t maxSteps,
                           const AnalysisResultsMap &analysisResults) {
    const BasicBlock *prevBlock = nullptr;
    const BasicBlock *currentBlock = startBlock;
    vector<BlockStep<const llvm::Value *>> steps;
    FastState currentState = entry;
    BlockUpdate<const llvm::Value *> update;
    uint32_t blocksVisited = 0;
    bool firstBlock = true;
    do {
        update =
            interpretBlock(*currentBlock, prevBlock, currentState, firstBlock,
                           maxSteps - blocksVisited, analysisResults);
        firstBlock = false;
        blocksVisited += update.blocksVisited;
        steps.emplace_back(currentBlock->getName(), std::move(update.step),
                           std::move(update.calls));
        prevBlock = currentBlock;
        currentBlock = update.nextBlock;
        if (blocksVisited > maxSteps || update.earlyExit) {
            return FastCall(&fun, std::move(entry), std::move(currentState),
                            std::move(steps), true, blocksVisited);
        }
    } while (currentBlock != nullptr);
    return FastCall(&fun, std::move(entry), std::move(currentState),
                    std::move(steps), false, blocksVisited);
}

FastCall interpretFunction(const Function &fun, FastState entry,
                           uint32_t maxSteps,
                           const AnalysisResultsMap &analysisResults) {
    return interpretFunction(fun, entry, &fun.getEntryBlock(), maxSteps,
                             analysisResults);
}

BlockUpdate<const llvm::Value *>
interpretBlock(const BasicBlock &block, const BasicBlock *prevBlock,
               FastState &state, bool skipPhi, uint32_t maxSteps,
               const AnalysisResultsMap &analysisResults) {
    uint32_t blocksVisited = 1;
    const Instruction *firstNonPhi = block.getFirstNonPHI();
    const Instruction *terminator = block.getTerminator();
    // Handle phi instructions
    BasicBlock::const_iterator instrIterator;
    for (instrIterator = block.begin(); &*instrIterator != firstNonPhi;
         ++instrIterator) {
        const Instruction *inst = &*instrIterator;
        assert(isa<PHINode>(inst));
        if (!skipPhi) {
            interpretPHI(*dyn_cast<PHINode>(inst), state, prevBlock);
        }
    }
    FastState step(state);

    vector<FastCall> calls;
    // Handle non phi instructions
    for (; &*instrIterator != terminator; ++instrIterator) {
        if (const auto call = dyn_cast<llvm::CallInst>(&*instrIterator)) {
            const Function *fun = call->getCalledFunction();
            FastVarMap args;
            auto argIt = fun->arg_begin();
            for (const auto &arg : call->arg_operands()) {
                args.insert(std::make_pair(
                    &*argIt, resolveValue(arg, state, arg->getType())));
                ++argIt;
            }
            FastCall c =
                interpretFunction(*fun, FastState(args, state.heap),
                                  maxSteps - blocksVisited, analysisResults);
            blocksVisited += c.blocksVisited;
            if (blocksVisited > maxSteps || c.earlyExit) {
                return BlockUpdate<const llvm::Value *>(
                    std::move(step), nullptr, std::move(calls), true,
                    blocksVisited);
            }
            state.heap = c.returnState.heap;
            insertOrReplace(
                state.variables,
                {call, c.returnState.variables
                           .find(analysisResults.at(fun).returnInstruction)
                           ->second});
            calls.push_back(std::move(c));
        } else {
            interpretInstruction(&*instrIterator, state);
        }
    }

    // Terminator instruction
    TerminatorUpdate update = interpretTerminator(block.getTerminator(), state);

    return BlockUpdate<const llvm::Value *>(std::move(step), update.nextBlock,
                                            std::move(calls), false,
                                            blocksVisited);
}

void interpretInstruction(const Instruction *instr, FastState &state) {
    if (const auto binOp = dyn_cast<BinaryOperator>(instr)) {
        interpretBinOp(binOp, state);
    } else if (const auto icmp = dyn_cast<ICmpInst>(instr)) {
        interpretICmpInst(icmp, state);
    } else if (const auto cast = dyn_cast<CastInst>(instr)) {
        assert(cast->getNumOperands() == 1);
        if (cast->getSrcTy()->isIntegerTy(1) &&
            cast->getDestTy()->getIntegerBitWidth() > 1) {
            // Convert a bool to an integer
            Integer operand = state.variables.find(cast->getOperand(0))->second;
            if (SMTGenerationOpts::getInstance().BitVect) {
                insertOrReplace(
                    state.variables,
                    {cast, Integer(makeBoundedInt(
                               cast->getType()->getIntegerBitWidth(),
                               unsafeBool(operand) ? 1 : 0))});
            } else {
                insertOrReplace(
                    state.variables,
                    {cast, Integer(mpz_class(unsafeBool(operand) ? 1 : 0))});
            }
        } else {
            if (const auto zext = dyn_cast<llvm::ZExtInst>(instr)) {
                insertOrReplace(
                    state.variables,
                    {zext,
                     resolveValue(zext->getOperand(0), state, zext->getType())
                         .zext(zext->getType()->getIntegerBitWidth())});
            } else if (const auto sext = dyn_cast<llvm::SExtInst>(instr)) {
                insertOrReplace(
                    state.variables,
                    {sext,
                     resolveValue(sext->getOperand(0), state, sext->getType())
                         .sext(sext->getType()->getIntegerBitWidth())});
            } else if (const auto trunc = dyn_cast<llvm::TruncInst>(instr)) {
                insertOrReplace(
                    state.variables,
                    {trunc,
                     resolveValue(trunc->getOperand(0), state, trunc->getType())
                         .zextOrTrunc(trunc->getType()->getIntegerBitWidth())});
            } else if (const auto ptrToInt =
                           dyn_cast<llvm::PtrToIntInst>(instr)) {
                insertOrReplace(
                    state.variables,
                    {ptrToInt,
                     resolveValue(ptrToInt->getPointerOperand(), state,
                                  ptrToInt->getPointerOperand()->getType())
                         .zextOrTrunc(
                             ptrToInt->getType()->getIntegerBitWidth())});
            } else if (const auto intToPtr =
                           dyn_cast<llvm::IntToPtrInst>(instr)) {
                insertOrReplace(
                    state.variables,
                    {ptrToInt, resolveValue(intToPtr->getOperand(0), state,
                                            intToPtr->getOperand(0)->getType())
                                   .zextOrTrunc(64)});
            } else {
                logErrorData("Unsupported instruction:\n", *instr);
                exit(1);
            }
        }
    } else if (const auto gep = dyn_cast<GetElementPtrInst>(instr)) {
        insertOrReplace(state.variables, {gep, resolveGEP(*gep, state)});
    } else if (const auto load = dyn_cast<LoadInst>(instr)) {
        Integer ptr = resolveValue(load->getPointerOperand(), state,
                                   load->getPointerOperand()->getType());
        // This will only insert 0 if there is not already a different element
        if (SMTGenerationOpts::getInstance().BitVect) {
            unsigned bytes = load->getType()->getIntegerBitWidth() / 8;
            llvm::APInt val =
                makeBoundedInt(load->getType()->getIntegerBitWidth(), 0);
            for (unsigned i = 0; i < bytes; ++i) {
                auto heapIt = state.heap.assignedValues.insert(std::make_pair(
                    ptr.asPointer() + Integer(mpz_class(i)).asPointer(),
                    Integer(makeBoundedInt(
                        8, state.heap.background.asUnbounded().get_si()))));
                assert(heapIt.first->second.type == IntType::Bounded);
                assert(heapIt.first->second.bounded.getBitWidth() == 8);
                val = (val << 8) |
                      (heapIt.first->second.bounded).sextOrSelf(bytes * 8);
            }
            insertOrReplace(state.variables, {load, Integer(val)});
        } else {
            auto heapIt = state.heap.assignedValues.insert(
                std::make_pair(ptr.asPointer(), state.heap.background));
            insertOrReplace(state.variables, {load, heapIt.first->second});
        }
    } else if (const auto store = dyn_cast<StoreInst>(instr)) {
        HeapAddress addr = resolveValue(store->getPointerOperand(), state,
                                        store->getPointerOperand()->getType());
        Integer val = resolveValue(store->getValueOperand(), state,
                                   store->getValueOperand()->getType());
        if (SMTGenerationOpts::getInstance().BitVect) {
            int bytes =
                store->getValueOperand()->getType()->getIntegerBitWidth() / 8;
            assert(val.type == IntType::Bounded);
            llvm::APInt bval = val.bounded;
            if (bytes == 1) {
                state.heap.assignedValues[addr] = val;
            } else {
                uint64_t i = 0;
                for (; bytes >= 0; --bytes) {
                    llvm::APInt el = bval.trunc(8);
                    bval = bval.ashr(8);
                    state.heap
                        .assignedValues[addr + Integer(llvm::APInt(
                                                   64, static_cast<uint64_t>(
                                                           bytes)))] =
                        Integer(el);
                    ++i;
                }
            }
        } else {
            state.heap.assignedValues[addr] = val;
        }
    } else if (const auto select = dyn_cast<SelectInst>(instr)) {
        Integer cond = resolveValue(select->getCondition(), state,
                                    select->getCondition()->getType());
        bool condVal = unsafeBool(cond);
        if (condVal) {
            Integer var =
                resolveValue(select->getTrueValue(), state, select->getType());
            insertOrReplace(state.variables, {select, var});
        } else {
            Integer var =
                resolveValue(select->getFalseValue(), state, select->getType());
            insertOrReplace(state.variables, {select, var});
        }

    } else {
        logErrorData("unsupported instruction:\n", *instr);
    }
}

void interpretPHI(const PHINode &instr, FastState &state,
                  const BasicBlock *prevBlock) {
    const Value *val = instr.getIncomingValueForBlock(prevBlock);
    Integer var = resolveValue(val, state, val->getType());
    insertOrReplace(state.variables, {&instr, var});
}

TerminatorUpdate interpretTerminator(const TerminatorInst *instr,
                                     FastState &state) {
    if (const auto retInst = dyn_cast<ReturnInst>(instr)) {
        if (retInst->getReturnValue() == nullptr) {
            insertOrReplace(state.variables, {retInst, Integer(mpz_class(0))});
        } else {
            insertOrReplace(
                state.variables,
                {retInst, resolveValue(retInst->getReturnValue(), state,
                                       retInst->getReturnValue()->getType())});
        }
        return TerminatorUpdate(nullptr);
    } else if (const auto branchInst = dyn_cast<BranchInst>(instr)) {
        if (branchInst->isUnconditional()) {
            assert(branchInst->getNumSuccessors() == 1);
            return TerminatorUpdate(branchInst->getSuccessor(0));
        } else {
            Integer cond = resolveValue(branchInst->getCondition(), state,
                                        branchInst->getCondition()->getType());
            bool condVal = unsafeBool(cond);
            assert(branchInst->getNumSuccessors() == 2);
            if (condVal) {
                return TerminatorUpdate(branchInst->getSuccessor(0));
            } else {
                return TerminatorUpdate(branchInst->getSuccessor(1));
            }
        }
    } else if (const auto switchInst = dyn_cast<SwitchInst>(instr)) {
        Integer condVal = resolveValue(switchInst->getCondition(), state,
                                       switchInst->getCondition()->getType());
        for (auto c : switchInst->cases()) {
            Integer caseVal;
            if (SMTGenerationOpts::getInstance().BitVect) {
                caseVal = Integer(c.getCaseValue()->getValue());
            } else {
                caseVal = Integer(mpz_class(c.getCaseValue()->getSExtValue()));
            }
            if (caseVal == condVal) {
                return TerminatorUpdate(c.getCaseSuccessor());
            }
        }
        return TerminatorUpdate(switchInst->getDefaultDest());

    } else {
        logError("Only return and branches are supported\n");
        return TerminatorUpdate(nullptr);
    }
}

Integer resolveValue(const Value *val, const FastState &state,
                     const llvm::Type * /* unused */) {
    if (isa<Instruction>(val) || isa<Argument>(val)) {
        return state.variables.find(val)->second;
    } else if (const auto constInt = dyn_cast<ConstantInt>(val)) {
        if (constInt->getBitWidth() == 1) {
            return Integer(constInt->getValue());
        } else if (!SMTGenerationOpts::getInstance().BitVect) {
            return Integer(mpz_class(constInt->getSExtValue()));
        } else {
            return Integer(constInt->getValue());
        }
    } else if (llvm::isa<llvm::ConstantPointerNull>(val)) {
        return Integer(makeBoundedInt(64, 0));
    }
    logErrorData("Operators are not yet handled\n", *val);
    exit(1);
}

void interpretICmpInst(const ICmpInst *instr, FastState &state) {
    assert(instr->getNumOperands() == 2);
    Integer op0 = resolveValue(instr->getOperand(0), state,
                               instr->getOperand(0)->getType());
    Integer op1 = resolveValue(instr->getOperand(1), state,
                               instr->getOperand(0)->getType());
    switch (instr->getPredicate()) {
    default:
        interpretIntPredicate(instr, instr->getPredicate(), std::move(op0),
                              std::move(op1), state);
    }
}

void interpretIntPredicate(const ICmpInst *instr, CmpInst::Predicate pred,
                           const Integer &i0, const Integer &i1,
                           FastState &state) {
    bool predVal = false;
    switch (pred) {
    case CmpInst::ICMP_EQ:
        predVal = i0.eq(i1);
        break;
    case CmpInst::ICMP_NE:
        predVal = i0.ne(i1);
        break;
    case CmpInst::ICMP_SGE:
        predVal = i0.sge(i1);
        break;
    case CmpInst::ICMP_SGT:
        predVal = i0.sgt(i1);
        break;
    case CmpInst::ICMP_SLE:
        predVal = i0.sle(i1);
        break;
    case CmpInst::ICMP_SLT:
        predVal = i0.slt(i1);
        break;
    case CmpInst::ICMP_UGE:
        predVal = i0.uge(i1);
        break;
    case CmpInst::ICMP_UGT:
        predVal = i0.ugt(i1);
        break;
    case CmpInst::ICMP_ULE:
        predVal = i0.ule(i1);
        break;
    case CmpInst::ICMP_ULT:
        predVal = i0.ult(i1);
        break;
    default:
        logErrorData("Unsupported predicate:\n", *instr);
    }
    insertOrReplace(state.variables, {instr, Integer(predVal)});
}

void interpretBinOp(const BinaryOperator *instr, FastState &state) {
    const Integer op0 = resolveValue(instr->getOperand(0), state,
                                     instr->getOperand(0)->getType());
    const Integer op1 = resolveValue(instr->getOperand(1), state,
                                     instr->getOperand(1)->getType());
    if (instr->getType()->getIntegerBitWidth() == 1) {
        bool b0 = unsafeBool(op0);
        bool b1 = unsafeBool(op1);
        interpretBoolBinOp(instr, instr->getOpcode(), b0, b1, state);
    } else {
        interpretIntBinOp(instr, instr->getOpcode(), std::move(op0),
                          std::move(op1), state);
    }
}

void interpretBoolBinOp(const BinaryOperator *instr, Instruction::BinaryOps op,
                        bool b0, bool b1, FastState &state) {
    bool result = false;
    switch (op) {
    case Instruction::Or:
        result = b0 || b1;
        break;
    case Instruction::And:
        result = b0 && b1;
        break;
    case Instruction::Xor:
        result = b0 ^ b1;
        break;
    default:
        logErrorData("Unsupported binop:\n", *instr);
        llvm::errs() << "\n";
    }
    insertOrReplace(state.variables, {instr, Integer(result)});
}

void interpretIntBinOp(const BinaryOperator *instr, Instruction::BinaryOps op,
                       const Integer &i0, const Integer &i1, FastState &state) {
    Integer result;
    switch (op) {
    case Instruction::Add:
        result = i0 + i1;
        break;
    case Instruction::Sub:
        result = i0 - i1;
        break;
    case Instruction::Mul:
        result = i0 * i1;
        break;
    case Instruction::SDiv:
        result = i0.sdiv(i1);
        break;
    case Instruction::UDiv:
        result = i0.udiv(i1);
        break;
    case Instruction::SRem:
        result = i0.srem(i1);
        break;
    case Instruction::URem:
        result = i0.urem(i1);
        break;
    case Instruction::Shl:
        result = i0.shl(i1);
        break;
    case Instruction::LShr:
        result = i0.lshr(i1);
        break;
    case Instruction::AShr:
        result = i0.ashr(i1);
        break;
    case Instruction::And:
        result = i0.and_(i1);
        break;
    case Instruction::Or:
        result = i0.or_(i1);
        break;
    case Instruction::Xor:
        result = i0.xor_(i1);
        break;
    default:
        logErrorData("Unsupported binop:\n", *instr);
        llvm::errs() << "\n";
    }
    insertOrReplace(state.variables, {instr, result});
}

bool varValEq(const Integer &lhs, const Integer &rhs) { return lhs == rhs; }

string valueName(const llvm::Value *val) { return val->getName(); }
template <typename T>
json stateToJSON(State<T> state, function<string(T)> getName) {
    map<string, json> jsonVariables;
    map<string, json> jsonHeap;
    for (const auto &var : state.variables) {
        string varName = getName(var.first);
        jsonVariables.insert({varName, toJSON(var.second)});
    }
    for (const auto &index : state.heap.assignedValues) {
        jsonHeap.insert({index.first.get_str(), index.second.get_str()});
    }
    json j;
    j["variables"] = jsonVariables;
    j["heap"] = jsonHeap;
    j["heapBackground"] = toJSON(state.heap.background);
    return j;
}
} // namespace dynamic
} // namespace llreve
