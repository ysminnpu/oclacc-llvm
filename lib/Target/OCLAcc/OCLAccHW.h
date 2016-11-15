#ifndef OCLACCHWPASS_H
#define OCLACCHWPASS_H

#include <unordered_map>

#include "llvm/IR/InstVisitor.h"
#include "llvm/Pass.h"

#include "OCLAccTargetMachine.h"
#include "HW/typedefs.h"
#include "HW/Design.h"
#include "HW/Kernel.h"

namespace llvm {

class Argument;
class AnalysisUsage;

class OCLAccHWVisitor;

class OCLAccHW : public ModulePass, public InstVisitor<OCLAccHW>{

  private:
    void createMakefile();

    void handleKernel(const Function &F);
    void handleArgument(const Argument &);

  public:
    OCLAccHW();
    ~OCLAccHW();

    virtual bool doInitialization(Module &);
    virtual bool doFinalization(Module &);

    virtual bool runOnModule(Module &);

    virtual void getAnalysisUsage(AnalysisUsage &AU) const;

    // Each LLVM value has to be mapped to an HW object. LLVM values may be
    // defined in a different Block, so ports have to be generated.

    typedef std::unordered_map<const Value *, oclacc::base_p> ValueMapTy;
    typedef ValueMapTy::iterator ValueMapIt;
    typedef ValueMapTy::const_iterator ValueMapConstIt;

    typedef std::unordered_map<const BasicBlock *, ValueMapTy> BlockValueMapTy;
    typedef BlockValueMapTy::iterator BlockValueMapIt;
    typedef BlockValueMapTy::const_iterator BlockValueMapConstIt;

    typedef std::map<const Function *, oclacc::kernel_p> KernelMapTy;
    typedef KernelMapTy::iterator KernelMapIt;
    typedef KernelMapTy::const_iterator KernelMapConstIt;

    typedef std::unordered_map<const BasicBlock *, oclacc::block_p> BlockMapTy;
    typedef BlockMapTy::iterator BlockMapIt;
    typedef BlockMapTy::const_iterator BlockMapConstIt;

    typedef std::unordered_map<const Argument *, oclacc::base_p> ArgMapTy;
    typedef ArgMapTy::iterator ArgMapIt;
    typedef ArgMapTy::const_iterator ArgMapConstIt;

    static char ID;

    oclacc::DesignUnit &getDesign() {
      return HWDesign;
    }


    // Visitor Methods
    void visitBasicBlock(BasicBlock &);
    void visitBinaryOperator(BinaryOperator &);
    void visitLoadInst(LoadInst &);
    void visitStoreInst(StoreInst &);
    void visitGetElementPtrInst(GetElementPtrInst &);
    void visitCallInst(CallInst &);
    void visitCmpInst(CmpInst &);
    //void visitICmpInst(ICmpInst &);
    //void visitFCmpInst(FCmpInst &);
    void visitPHINode(PHINode &);

  private:
    KernelMapTy KernelMap;
    BlockMapTy BlockMap;
    BlockValueMapTy BlockValueMap;
    ArgMapTy ArgMap;

    oclacc::DesignUnit HWDesign;

    oclacc::const_p makeConstant(Constant *, Instruction *);

    template<class HW, class ...Args>
    std::shared_ptr<HW> makeHWBB(const BasicBlock *BB, const Value *IR, Args&& ...args) {

      // A single Constant value might be used by
      // multiple instructions, leading to conflicts in the Map.
      if (isa<Constant>(IR))
        llvm_unreachable("Do not use makeHW to create constants.");

      // Arguments have no  matching BasicBlock, they must not be added to the
      // BlockValueMap but to the ArgMap.
      if (isa<Argument>(IR))
        llvm_unreachable("Do not use makeHW to create arguments.");

      std::shared_ptr<HW> HWP = std::make_shared<HW>(args...);
      HWP->setIR(IR);
      BlockValueMap[BB][IR] = HWP;

      // Add new HW object to current Block
      BlockMapIt BI = BlockMap.find(BB);
      if (BI == BlockMap.end()) {
        BB->dump();
        IR->dump();
        llvm_unreachable("No Block");
      }
      oclacc::block_p HWB = BI->second;
      HWB->addOp(HWP);
      HWP->setParent(HWB);

      return HWP;
    }

    template<class HW, class ...Args>
    std::shared_ptr<HW> makeHW(const Value *IR, Args&& ...args) {
      if (const Instruction *I = dyn_cast<Instruction>(IR))
        return makeHWBB<HW>(I->getParent(), IR, args...);

      // Arguments have no BasicBlock. 
      // No need to assign them to the kernel. This is done by handleArgument(),
      // as we do not know if it is an In or Out Port.
      else if (const Argument *I = dyn_cast<Argument>(IR)) {
        std::shared_ptr<HW> HWP = std::make_shared<HW>(args...);
        HWP->setIR(IR);
        ArgMap[I] = HWP;
        return HWP;
      }
      else {
        IR->dump();
        llvm_unreachable("Called makeHW on non-Instruction, non-Argument value");
      }
    }


    /// \brief Make new kernel and add to KernelMap
    template<class ...Args>
    oclacc::kernel_p makeKernel(const Function *IR, Args&& ...args) {
      oclacc::kernel_p HWP = std::make_shared<oclacc::Kernel>(args...);
      KernelMap[IR] = HWP;
      return HWP;
    }

    /// \brief Make new Block and add to current kernel
    template<class ...Args>
    oclacc::block_p makeBlock(const BasicBlock *BB, Args&& ...args) {
      oclacc::block_p HWB = std::make_shared<oclacc::Block>(args...);
      BlockMap[BB] = HWB;

      // add block to kernel
      const Function *F = BB->getParent();
      KernelMapIt KI = KernelMap.find(F);
      if (KI == KernelMap.end()) {
        F->dump();
        llvm_unreachable("No Kernel");
      }
      oclacc::kernel_p HWK = KI->second;
      HWK->addBlock(HWB);
      HWB->setParent(HWK);

      return HWB;
    }

    template<class HW>
    std::shared_ptr<HW> getHW(const BasicBlock *BB, const Value *IR) const {
      assert(! isa<Argument>(IR) && "Arguments are not assigned to a BasicBlock");

      BlockValueMapConstIt BI = BlockValueMap.find(BB);
      if (BI == BlockValueMap.end()) {
        BB->dump();
        IR->dump(); 
        llvm_unreachable("No BB");
      }

      ValueMapConstIt VI = BI->second.find(IR);
      if (VI == BI->second.end()) {
        BB->dump();
        IR->dump(); 
        llvm_unreachable("No HW");
      }

      std::shared_ptr<HW> P = std::static_pointer_cast<HW>(VI->second);
      return P;
    }

    /// \brief Return HW object for initial definition of IR.
    /// Generally use getHW(const BasicBlock *BB, ...) 
    template<class HW> std::shared_ptr<HW> getHW(const Value *IR) const {
      if (const Instruction *I = dyn_cast<Instruction>(IR))
        return getHW<HW>(I->getParent(), IR);
      else if (const Argument *I = dyn_cast<Argument>(IR)) {
        ArgMapConstIt AI = ArgMap.find(I);
        if (AI == ArgMap.end()) {
          I->dump();
          llvm_unreachable("No Arg");
        }

        std::shared_ptr<HW> P = std::static_pointer_cast<HW>(AI->second);
        return P;
      }
      else {
        IR->dump();
        llvm_unreachable("Called getHW on non-Instruction, non-Argument value");
      }
    }

    oclacc::block_p getBlock(const BasicBlock *BB) const {
      BlockMapConstIt VI = BlockMap.find(BB);
      if (VI == BlockMap.end()) {
        errs() << BB->getName() << "\n";
        llvm_unreachable("No Block");
      }

      return VI->second;
    }

    oclacc::kernel_p getKernel(const Function *F) const {
      KernelMapConstIt VI = KernelMap.find(F);
      if (VI == KernelMap.end()) {
        errs() << F->getName() << "\n";
        llvm_unreachable("No Kernel");
      }

      return VI->second;
    }

    void connect(oclacc::base_p HWFrom, oclacc::base_p HWTo) {
      HWFrom->addOut(HWTo);
      HWTo->addIn(HWFrom);
    }

    oclacc::Datatype getDatatype(Type *T) {
      oclacc::Datatype DT=oclacc::Invalid;
      if (T->isIntegerTy()) DT=oclacc::Integer;
      else if (T->isHalfTy()) DT=oclacc::Half;
      else if (T->isFloatTy()) DT=oclacc::Float;
      else if (T->isDoubleTy()) DT = oclacc::Double;
      else {
        T->dump();
        llvm_unreachable("Invalid Type");
      }

      return DT;
    }
};

} //end namespace llvm

#endif /* OCLACCHWPASS_H */
