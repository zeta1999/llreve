#include "RemoveMarkPass.h"

llvm::PreservedAnalyses RemoveMarkPass::run(llvm::Function &Fun,
                                            llvm::FunctionAnalysisManager *AM) {
    auto BidirMarkBlockMap = AM->getResult<MarkAnalysis>(Fun);
    std::set<llvm::Instruction *> ToDelete;
    for (auto BBTuple : BidirMarkBlockMap.MarkToBlocksMap) {
        // no need to remove anything in exit and entry nodes
        if (BBTuple.first >= 0) {
            for (auto BB : BBTuple.second) {
                for (auto &Inst : *BB) {
                    if (auto CallInst = llvm::dyn_cast<llvm::CallInst>(&Inst)) {
                        if ((CallInst->getCalledFunction() != nullptr) &&
                            CallInst->getCalledFunction()->getName() ==
                                "__mark") {
                            ToDelete.insert(CallInst);
                        }
                    }
                }
            }
        }
    }
    for (auto Instr : ToDelete) {
        for (auto User : Instr->users()) {
            if (auto UserInstr = llvm::dyn_cast<llvm::Instruction>(User)) {
                // handle and
                if (auto BinOp =
                        llvm::dyn_cast<llvm::BinaryOperator>(UserInstr)) {
                    if (BinOp->getOpcode() == llvm::Instruction::And) {
                        removeAnd(Instr, BinOp);
                    }
                }
            }
        }
        // kill the call instruction
        Instr->eraseFromParent();
    }
    return llvm::PreservedAnalyses::all();
}

void removeAnd(llvm::Instruction *Instr, llvm::BinaryOperator *BinOp) {
    llvm::Value *ReplaceVal = nullptr;
    // find non dummy operand
    if (Instr == BinOp->getOperand(0)) {
        ReplaceVal = BinOp->getOperand(1);
    } else if (Instr == BinOp->getOperand(1)) {
        ReplaceVal = BinOp->getOperand(0);
    }

    assert(ReplaceVal);

    // replace all uses of and with the operand
    auto Replace = llvm::dyn_cast<llvm::User>(ReplaceVal);
    if (Replace == nullptr) {
        return;
    }
    for (auto &Use : BinOp->uses()) {
        if (auto Used = llvm::dyn_cast<llvm::User>(Use.getUser())) {
            Used->replaceUsesOfWith(BinOp, Replace);
        }
    }
    BinOp->eraseFromParent();

    // try to remove the zext and all uses of it
    auto ZExt = llvm::dyn_cast<llvm::ZExtInst>(Replace);
    if (ZExt == nullptr) {
        return;
    }
    for (auto User : ZExt->users()) {
        if (auto UserInstr = llvm::dyn_cast<llvm::Instruction>(User)) {
            for (auto UserOfUser : User->users()) {
                if (auto UserOfUserInstr =
                        llvm::dyn_cast<llvm::User>(UserOfUser)) {
                    UserOfUserInstr->replaceUsesOfWith(User,
                                                       ZExt->getOperand(0));
                }
            }
            UserInstr->eraseFromParent();
        }
    }
    ZExt->eraseFromParent();
}
