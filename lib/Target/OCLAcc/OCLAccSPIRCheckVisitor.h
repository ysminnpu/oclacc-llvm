#ifndef OCLACCSPIRCHECKVISITOR_H
#define OCLACCSPIRCHECKVISITOR_H

#include "llvm/IR/Instruction.h"
#include "llvm/IR/InstVisitor.h"

#include <map>
#include <set>
#include <list>
#include <utility>

#include "OCLAccMacros.h"
#include "HW/typedefs.h"

namespace llvm {

struct Value3D {
  int64_t X;
  int64_t Y;
  int64_t Z;
};

class OCLAccSPIRCheckVisitor : public InstVisitor<OCLAccSPIRCheckVisitor> {
  private:
    bool _64Bit               = false;
    std::string ModuleName    = "defaultModule";

    std::list<std::string> NonConformingExpr;
    std::map<const std::string, Function *> KernelMap;

    Value3D WorkGroupSizeHint = {0,0,0};
    Value3D ReqdWorkGroupSize = {0,0,0};

    void extractKernelInfo(const Module &F);

  public:
    OCLAccSPIRCheckVisitor();

    OCLAccSPIRCheckVisitor(const OCLAccSPIRCheckVisitor &) = delete;
    OCLAccSPIRCheckVisitor &operator =(const OCLAccSPIRCheckVisitor &) = delete;


    /* 
     * Inline Methods
     */
    bool isConform() const {
      return NonConformingExpr.empty();  
    }

    bool is64Bit() const {
      return _64Bit;
    }

    const std::string &getModuleName() const {
      return ModuleName;
    }

    const std::map<const std::string, Function *> &getKernels() const {
      return KernelMap;
    }

    /*
     * Helper Methods
     */

    /* 
     * Visit Methods 
     * */
    void visitModule(Module &F);
    void visitFunction(Function &F);
    void visitBasicBlock(BasicBlock &BB);

    /* Terminator */
    void visitReturnInst(ReturnInst &I);
    void visitBranchInst(BranchInst &I);
    void visitSwitchInst(SwitchInst &I);
    void visitIndirectBrInst(IndirectBrInst &I) { llvm_unreachable("not supported"); }
    void visitInvokeInst(InvokeInst &I) { llvm_unreachable("not supported"); }
    /* part of SPIR but not supported by LLVM any more/
     * void visitUnwindInst(UnwindInst &I) { llvm_unreachable("not supported"); }
     */
    void visitResumeInst(ResumeInst &I) { llvm_unreachable("not supported"); }
    void visitUnreachableInst(UnreachableInst &I);

    /* Binary */
    void visitBinaryOperator(Instruction &I);

    /*
     * Deactivated to use common visit method
     */
#if 0
    void visitAdd(BinaryOperator &I);
    void visitFAdd(BinaryOperator &I);
    void visitSub(BinaryOperator &I);
    void visitFSub(BinaryOperator &I);
    void visitMul(BinaryOperator &I);
    void visitFMul(BinaryOperator &I);
    void visitUDiv(BinaryOperator &I);
    void visitSDiv(BinaryOperator &I);
    void visitFDiv(BinaryOperator &I);
    void visitURem(BinaryOperator &I);
    void visitSRem(BinaryOperator &I);

    /* Bitwise Binary */
    void visitShl(BinaryOperator &I);
    void visitLShr(BinaryOperator &I);
    void visitAShr(BinaryOperator &I);
    void visitAnd(BinaryOperator &I);
    void visitOr(BinaryOperator &I);
    void visitXor(BinaryOperator &I);
#endif

    /* Vector */
    void visitExtractElementInst(ExtractElementInst &I);
    void visitInsertElementInst(InsertElementInst &I);
    void visitShuffleVectorInst(ShuffleVectorInst &I);

    /* Aggregate */
    void visitExtractValueInst(ExtractValueInst &I);
    void visitInsertValueInst(InsertValueInst &I);

    /* Memory Addressing */
    void visitAllocaInst(AllocaInst &I);
    void visitLoadInst(LoadInst &I);
    void visitStoreInst(StoreInst &I);
    void visitFenceInst(FenceInst &I) { llvm_unreachable("not supported"); }
    void visitAtomicCmpXchgInst(AtomicCmpXchgInst &I) { llvm_unreachable("not supported"); }
    void visitAtomicRMWInst(AtomicRMWInst &I) { llvm_unreachable("not supported"); }
    void visitGetElementPtrInst(GetElementPtrInst &I);

    /* Conversion Operations */
    void visitTruncInst(TruncInst &I);
    void visitZExtInst(ZExtInst &I);
    void visitSExtInst(SExtInst &I);
    void visitFPTruncInst(FPTruncInst &I);
    void visitFPExtInst(FPExtInst &I);
    void visitFPToUIInst(FPToUIInst &I);
    void visitFPToSIInst(FPToSIInst &I);
    void visitUIToFPInst(UIToFPInst &I);
    void visitSIToFPInst(SIToFPInst &I);
    void visitPtrToIntInst(PtrToIntInst &I);
    void visitIntToPtrInst(IntToPtrInst &I);
    void visitBitCastInst(BitCastInst &I);
    void visitAddrSpaceCastInst(AddrSpaceCastInst &I);

    /* Other Operations */
    void visitICmpInst(ICmpInst &I);
    void visitFCmpInst(FCmpInst &I);
    void visitPHINode(PHINode &I);
    void visitSelectInst(SelectInst &I);
    void visitCallInst(CallInst &I);
    void visitVAArgInst(VAArgInst &I) { llvm_unreachable("not supported"); }
    void visitLandingPadInst(LandingPadInst &I) { llvm_unreachable("not supported"); }

    void visitInlineAsm(InlineAsm &I) { llvm_unreachable("not supported"); }

    /* Aggegated visits */
    void visitCmpInst(CmpInst &I);
    void visitTerminatorInst(TerminatorInst &);
    void visitUnaryInstruction(UnaryInstruction &);
    void visitArgument(Argument &I);
    void visitInstruction(Instruction &I);
};

} //end namespace llvm

#endif /* OCLACCSPIRCHECKVISITOR_H */
