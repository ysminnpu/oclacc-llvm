#ifndef OCLACCHWVISITOR_H
#define OCLACCHWVISITOR_H


#include "llvm/IR/Instruction.h"
#include "llvm/IR/InstVisitor.h"

#include <map>
#include <set>
#include <utility>

#include "HW/typedefs.h"
#include "HW/Kernel.h"


namespace llvm {

///
///Generate HW-tree from SPIR
///
///-# Find kernels and analyze find pointer types (Streams)
///-# Locate store targets. If in Streams -> OutputStream
///-# Locate load  targets. If in Streams -> InputStream
///
class OCLAccHWVisitor : public InstVisitor<OCLAccHWVisitor> {
  private:

    const Function *KernelFunction;
    oclacc::kernel_p HWKernel;

    BasicBlock * CurrentBB;
    oclacc::block_p CurrentHWBlock;

    std::map<BasicBlock *, oclacc::block_p> BBMap;
    typedef std::map<BasicBlock *, oclacc::block_p>::iterator BBMapIt;

    //
    // LLVM Control Flow
    //
    std::map<const Value *, oclacc::base_p> ValueMap;
    typedef std::map<const Value *, oclacc::base_p>::iterator ValueMapIt;

    //moved to HW/Control
    //enum ConditionFlag {COND_TRUE, COND_FALSE};
    //

    std::map<std::pair<BasicBlock *, BasicBlock *>, std::pair<Value *, oclacc::ConditionFlag > > CondMap;
    typedef std::map<std::pair<BasicBlock *, BasicBlock *>, std::pair<Value *, oclacc::ConditionFlag > >::iterator CondMapIt;

    //std::map<BasicBlock *, std::set<Value *> > BlockInputs;
    //std::map<BasicBlock *, std::set<Value *> > BlockOutputs;
    //typedef std::map<BasicBlock *, std::list<Value*> >::iterator BlockInOutIt;

    std::set<std::pair<BasicBlock *, BasicBlock *>> UncondSet;
    typedef std::set<std::pair<BasicBlock *, BasicBlock *> >::iterator UncondSetIt;

    //
    // Hardware-relevant
    //
    std::map<std::pair<BasicBlock *, BasicBlock *>, oclacc::base_p> HWThenMap;
    std::map<std::pair<BasicBlock *, BasicBlock *>, oclacc::base_p> HWElseMap;
    typedef std::map<std::pair<BasicBlock *, BasicBlock *>, oclacc::base_p>::iterator HWMapIt;


    /* 
     * Helper Functions
     */
    oclacc::const_p createConstant(Constant *C);

    oclacc::block_p createHWBlock(BasicBlock* BB);

  public:
    OCLAccHWVisitor(oclacc::kernel_p K);

    oclacc::kernel_p getKernel() {
      return HWKernel;
    }

    /* These functions will be defined in a later Version of llvm
     */
    BasicBlock *getSingleSuccessor(BasicBlock *);
    BasicBlock *getUniqueSuccessor(BasicBlock *);

    size_t getScalarBitWidth(Value *V);

    void visitBasicBlock(BasicBlock &I);
    void visitArgument(const Argument &I);

    void visitBinaryOperator(Instruction &I);
    //void visitAdd(BinaryOperator &I);
    //void visitFAdd(BinaryOperator &I);
    //void visitMul(BinaryOperator &I);
    //void visitFMul(BinaryOperator &I);

    void visitReturnInst(ReturnInst &I);
    void visitBranchInst(BranchInst &I);
    void visitSwitchInst(SwitchInst &I);
    //void visitIndirectBrInst(IndirectBrInst &I);
    //void visitInvokeInst(InvokeInst &I) {
    //  llvm_unreachable("Lowerinvoke pass didn't work!");
    //}
    //void visitResumeInst(ResumeInst &I) {
    //  llvm_unreachable("DwarfEHPrepare pass didn't work!");
    //}
    //void visitUnreachableInst(UnreachableInst &I);

    void visitCmpInst(CmpInst &I);
    //void visitICmpInst(ICmpInst &I);
    //void visitFCmpInst(FCmpInst &I);
    void visitTruncInst(TruncInst &I);

    //void visitCastInst (CastInst &I);
    //void visitSelectInst(SelectInst &I);
    void visitCallInst (CallInst &I);
    //void visitInlineAsm(CallInst &I);
    //bool visitBuiltinCall(CallInst &I, Intrinsic::ID ID, bool &WroteCallee);

    //void visitAllocaInst(AllocaInst &I);
    void visitLoadInst(LoadInst &I);
    void visitStoreInst(StoreInst &I);
    void visitGetElementPtrInst(GetElementPtrInst &I);
    //void visitVAArgInst (VAArgInst &I);
    void visitPHINode(PHINode &I);

    void visitInsertElementInst(InsertElementInst &I);
    //void visitExtractElementInst(ExtractElementInst &I);
    //void visitShuffleVectorInst(ShuffleVectorInst &SVI);

    //void visitInsertValueInst(InsertValueInst &I);
    //void visitExtractValueInst(ExtractValueInst &I);

    void visitInstruction(Instruction &I) {
      errs() << "NOT_IMPLEMENTED: " << I << " " << I.getValueID() << "\n";
      llvm_unreachable(0);
    }

    /* 
     * Handle BuiltIn Functions
     */
    void handleBuiltinWorkItem(CallInst &I, const std::string &FunctionName);
    void handleBuiltinMath(CallInst &I, const std::string &FunctionName);
    void handleBuiltinInteger(CallInst &I, const std::string &FunctionName);
    void handleBuiltinWorkGroupBarrier(CallInst &I, const std::string &FunctionName);
    void handleBuiltinToGlobal(CallInst &I, const std::string &FunctionName);
    void handleBuiltinToLocal(CallInst &I, const std::string &FunctionName);
    void handleBuiltinToPrivate(CallInst &I, const std::string &FunctionName);
    void handleBuiltinGetFence(CallInst &I, const std::string &FunctionName);
    void handleBuiltinPrintf(CallInst &I, const std::string &FunctionName);
};

} //end namespace llvm

#endif /* OCLACCHWVISITOR_H */
