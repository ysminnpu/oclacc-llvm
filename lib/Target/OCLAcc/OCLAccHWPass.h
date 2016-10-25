#ifndef OCLACCHWPASS_H
#define OCLACCHWPASS_H

#include "llvm/IR/InstVisitor.h"
#include "llvm/Pass.h"

#include "OCLAccTargetMachine.h"
#include "HW/typedefs.h"
#include "HW/Design.h"

namespace llvm {

class Argument;
class AnalysisUsage;

class OCLAccHWVisitor;

class OCLAccHWPass : public ModulePass, public InstVisitor<OCLAccHWPass>{

  private:
    void createMakefile();

    void handleKernel(const Function &F);
    void handleArgument(const Argument &);
    void handleBBPorts(const BasicBlock &B);

  public:
    OCLAccHWPass();
    ~OCLAccHWPass();

    OCLAccHWPass (const OCLAccHWPass &) = delete;
    OCLAccHWPass &operator =(const OCLAccHWPass &) = delete;

    OCLAccHWPass *createOCLAccHWPass();

    virtual const char *getPassName() const { return "OCLAcc OCLAccHWPass"; }

    virtual bool doInitialization(Module &);
    virtual bool doFinalization(Module &);

    virtual bool runOnModule(Module &);

    virtual void getAnalysisUsage(AnalysisUsage &AU) const;

    typedef std::map<const Value *, oclacc::base_p> ValueMapType;
    typedef ValueMapType::iterator ValueMapIt;
    typedef ValueMapType::const_iterator ValueMapConstIt;

    typedef std::map<const Function *, oclacc::kernel_p> KernelMapType;
    typedef KernelMapType::iterator KernelMapIt;
    typedef KernelMapType::const_iterator KernelMapConstIt;

    typedef std::map<const BasicBlock *, oclacc::block_p> BlockMapType;
    typedef BlockMapType::iterator BlockMapIt;
    typedef BlockMapType::const_iterator BlockMapConstIt;

    static char ID;

    oclacc::DesignUnit &getDesign() {
      return HWDesign;
    }


    // Visitor Methods
    void visitBinaryOperator(Instruction &I);
    void visitLoadInst(LoadInst &I);
    void visitStoreInst(StoreInst &I);
    void visitGetElementPtrInst(GetElementPtrInst &I);
    void visitCallInst(CallInst &I);

  private:
    ValueMapType ValueMap;
    KernelMapType KernelMap;
    BlockMapType BlockMap;

    // FIXME Instantiate Design independent of Pass
    oclacc::DesignUnit HWDesign;

    oclacc::const_p createConstant(Constant *, BasicBlock *B, oclacc::Datatype);

    template<class HW, class ...Args>
    std::shared_ptr<HW> makeHW(const Value *IR, Args&& ...args) {
      std::shared_ptr<HW> P = std::make_shared<HW>(args...);
      P->setIR(IR);
      ValueMap[IR] = P;
      return P;
    }

    template<class ...Args>
    std::shared_ptr<oclacc::Kernel> makeKernel(const Function *IR, Args&& ...args) {
      std::shared_ptr<oclacc::Kernel> P = std::make_shared<oclacc::Kernel>(args...);
      KernelMap[IR] = P;
      return P;
    }

    template<class ...Args>
    std::shared_ptr<oclacc::Block> makeBlock(const BasicBlock *IR, Args&& ...args) {
      std::shared_ptr<oclacc::Block> P = std::make_shared<oclacc::Block>(args...);
      BlockMap[IR] = P;
      return P;
    }

    template<class HW>
    std::shared_ptr<HW> getHW(const Value *IR) const {
      ValueMapConstIt VI = ValueMap.find(IR);
      if (VI == ValueMap.end()) {
        errs() << IR->getName() << "\n";
        llvm_unreachable("No HW");
      }

      std::shared_ptr<HW> P = std::static_pointer_cast<HW>(VI->second);
      return P;
    }

    std::shared_ptr<oclacc::Block> getBlock(const BasicBlock *B) const {
      BlockMapConstIt VI = BlockMap.find(B);
      if (VI == BlockMap.end()) {
        errs() << B->getName() << "\n";
        llvm_unreachable("No Block");
      }

      return VI->second;
    }

    std::shared_ptr<oclacc::Kernel> getKernel(const Function *F) const {
      KernelMapConstIt VI = KernelMap.find(F);
      if (VI == KernelMap.end()) {
        errs() << F->getName() << "\n";
        llvm_unreachable("No Kernel");
      }

      return VI->second;
    }

    void connect(oclacc::base_p HWFrom, oclacc::base_p HWTo) {
      HWFrom->appOut(HWTo);
      HWTo->appIn(HWFrom);
    }
};

} //end namespace llvm

#endif /* OCLACCHWPASS_H */
