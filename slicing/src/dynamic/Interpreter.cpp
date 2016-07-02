#include "Interpreter.h"

#include "core/Util.h"

#include "llvm/IR/Argument.h"
#include "llvm/IR/InstrTypes.h"
#include "llvm/IR/Instructions.h"

#include <cassert>
#include <iostream>

using namespace std;
using namespace llvm;

Interpreter::Interpreter(
		Function const& func,
		uint64_t const  input[]) :
	func        (func),
	_instIt     (func.getEntryBlock().begin()),
	_pRecentInst(nullptr),
	_pNextInst  (&*_instIt) {
	
	unsigned int curIndex = 0;
	
	// Initialize the argument values with the input passed as argument
	for(Argument const& i : func.getArgumentList()) {
		_state[&i] = new APInt(getValueBitWidth(i), input[curIndex++]);
	}
	
	// Initialize the instruction values with undefined values
	for(Instruction const& i : Util::getInstructions(func)) {
		if(isIntValue(i)) {
			_state[&i] = &valueUndef;
		}
	}
}

Interpreter::~Interpreter(void) {
	
	// Delete the integers allocated on the heap;
	for(auto const& i : _trace) {
		if(i.second != &valueVoid && i.second != &valueUndef) {
			delete i.second;
		}
	}
	
	// Delete the return value
	if(_pRetValue && _pRetValue != &valueVoid) {
		delete _pRetValue;
	}
}

bool Interpreter::execute(
		unsigned long const maxStepCount) {
	
	unsigned long stepCounter = 0;
	
	while(_pNextInst && stepCounter < maxStepCount) {
		executeNextInstruction();
		stepCounter++;
	}
	
	return _pNextInst;
}

bool Interpreter::executeNextInstruction(void) {
	
	// Check whether the end has already been reached
	if(!_pNextInst) return false;
	
	APInt const* pNewValue = nullptr;
	
	if(isa<BinaryOperator>(_pNextInst)) {
		pNewValue = new APInt(executeBinaryOperator());
	} else if(isa<BranchInst>(_pNextInst)) {
		executeBranchInst();
	} else if(isa<CallInst>(_pNextInst)) {
		if(isIntValue(*_pNextInst)) {
			pNewValue = new APInt(executeCallInst());
		} else {
			executeCallInst();
		}
	} else if(isa<ICmpInst>(_pNextInst)) {
		pNewValue = new APInt(1, executeICmpInst() ? 1 : 0);
	} else if(isa<PHINode>(_pNextInst)) {
		pNewValue = new APInt(executePHINode());
	} else if(isa<ReturnInst>(_pNextInst)) {
		executeReturnInst();
	} else {
		assert(false);
	}
	
	// Update state and execution trace
	_state[_pNextInst] = pNewValue;
	_trace.push_back({_pNextInst, pNewValue});
	
	moveToNextInstruction();
	
	return pNewValue;
}

Instruction const* Interpreter::getNextInstruction(void) const {
	
	return _pNextInst;
}

Instruction const* Interpreter::getRecentInstruction(void) const {
	
	return _pRecentInst;
}

APInt const* Interpreter::getReturnValue(void) const {
	
	return _pRetValue;
}

unsigned int Interpreter::getValueBitWidth(
		Value const& value) const {
	
	assert(isIntValue(value));
	
	return value.getType()->getPrimitiveSizeInBits();
}

bool Interpreter::isIntValue(
		Value const& value) const {
	
	if(value.getType()->isIntegerTy()) {
		return true;
	} else {
		assert(value.getType()->isVoidTy());
		return false;
	}
}

void Interpreter::moveToNextInstruction(void) {
	
	_pRecentInst = _pNextInst;
	
	if(isa<ReturnInst>(_pNextInst)) {
		
		_pNextInst = nullptr;
		
	} else {
		
		if(!isa<TerminatorInst>(_pNextInst)) {
			++_instIt;
		}
		
		_pNextInst = &*_instIt;
	}
}

APInt Interpreter::resolveValue(
		Value const* pVal) const {
	
	if(isa<Instruction>(pVal) || isa<Argument>(pVal)) {
		
		return *_state.at(pVal);
		
	} else if (auto const constInt = dyn_cast<ConstantInt>(pVal)) {
		
		// For the moment treat all as integers, no matter which bit width
		return constInt->getValue();
		
	//} else if (isa<ConstantPointerNull>(val)) {
		//return make_shared<VarInt>(Integer(makeBoundedInt(64, 0)));
	} else {
		
		assert(false);
	}
}

APInt const& Interpreter::operator[](
		Value const& value) const {
	
	try {
		return *_state.at(&value);
	} catch(out_of_range e) {
		return valueVoid;
	}
}

APInt Interpreter::executeBinaryOperator(void) {
	
	assert(_pNextInst->getNumOperands() == 2);
	
	// Get the operands and the variable belonging to this instruction
	APInt const op1 = resolveValue(_pNextInst->getOperand(0));
	APInt const op2 = resolveValue(_pNextInst->getOperand(1));
	
	switch(_pNextInst->getOpcode()) {
		case Instruction::Add:  return op1 + op2;
		case Instruction::Sub:  return op1 - op2;
		case Instruction::Mul:  return op1 * op2;
		case Instruction::And:  return op1 & op2;
		case Instruction::Or:   return op1 | op2;
		case Instruction::Xor:  return op1 ^ op2;
		case Instruction::SDiv: return op1.sdiv(op2);
		case Instruction::UDiv: return op1.udiv(op2);
		case Instruction::SRem: return op1.srem(op2);
		case Instruction::URem: return op1.urem(op2);
		case Instruction::Shl:  return op1.shl (op2);
		case Instruction::LShr: return op1.lshr(op2);
		case Instruction::AShr: return op1.ashr(op2);
		default:
			assert(false);
			return valueUndef;
	}
}

void Interpreter::executeBranchInst(void) {
	
	BranchInst const& branchInst = *dyn_cast<BranchInst const>(_pNextInst);
	unsigned int      successorIndex;
	
	if(branchInst.isUnconditional()) {
		
		assert(branchInst.getNumSuccessors() == 1);
		successorIndex = 0;
		
	} else {
		
		assert(branchInst.getNumSuccessors() == 2);
		successorIndex =
			resolveValue(branchInst.getCondition()).getBoolValue() ? 0 : 1;
	}
	
	// Move to the next basic block
	_instIt = branchInst.getSuccessor(successorIndex)->begin();
}

APInt Interpreter::executeCallInst(void) {
	
	CallInst const& callInst = *dyn_cast<CallInst>(_pNextInst);
	
	assert(callInst.getCalledFunction() != nullptr);
	
	unsigned int const argCount = callInst.getNumArgOperands();
	uint64_t*    const args     = new uint64_t[argCount];
	
	// Collect arguments
	for(unsigned int i = 0; i < argCount; i++) {
		args[i] = resolveValue(callInst.getArgOperand(i)).getLimitedValue();
	}
	
	Interpreter interpreter(*callInst.getCalledFunction(), args);
	
	// Interprete the function
	interpreter.execute();
	
	// Free resources for the argument array
	delete [] args;
	
	return *interpreter.getReturnValue();
}

bool Interpreter::executeICmpInst(void) {
	
	assert(_pNextInst->getNumOperands() == 2);
	
	// Get the operands and the variable belonging to this instruction
	APInt const op1 = resolveValue(_pNextInst->getOperand(0));
	APInt const op2 = resolveValue(_pNextInst->getOperand(1));
	
	switch(dyn_cast<CmpInst>(_pNextInst)->getPredicate()) {
		case CmpInst::ICMP_EQ:  return op1.eq (op2);
		case CmpInst::ICMP_NE:  return op1.ne (op2);
		case CmpInst::ICMP_SGE: return op1.sge(op2);
		case CmpInst::ICMP_SGT: return op1.sgt(op2);
		case CmpInst::ICMP_SLE: return op1.sle(op2);
		case CmpInst::ICMP_SLT: return op1.slt(op2);
		case CmpInst::ICMP_UGE: return op1.uge(op2);
		case CmpInst::ICMP_UGT: return op1.ugt(op2);
		case CmpInst::ICMP_ULE: return op1.ule(op2);
		case CmpInst::ICMP_ULT: return op1.ult(op2);
		default:
			assert(false);
			return false;
	}
}

APInt Interpreter::executePHINode(void) {
	
	return resolveValue(
		dyn_cast<PHINode>(_pNextInst)->
		getIncomingValueForBlock(_pRecentInst->getParent()));
}

void Interpreter::executeReturnInst(void) {
	
	Value const* const pRetValue =
		dyn_cast<ReturnInst>(_pNextInst)->getReturnValue();
	
	_pRetValue = pRetValue ? new APInt(resolveValue(pRetValue)) : &valueVoid;
}
