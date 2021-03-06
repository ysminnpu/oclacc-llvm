#ifndef OCLACCHWPASS_H
#define OCLACCHWPASS_H

#include <unordered_map>

#include "llvm/IR/InstVisitor.h"
#include "llvm/IR/Operator.h"
#include "llvm/Pass.h"

#include "OCLAccTargetMachine.h"
#include "HW/typedefs.h"
#include "HW/Design.h"
#include "HW/Kernel.h"
#include "HW/Port.h"

namespace llvm {

class Argument;
class AnalysisUsage;

class OCLAccHWVisitor;

class OCLAccHW : public ModulePass, public InstVisitor<OCLAccHW>{

  private:
    void createMakefile();

    // Helper
    void handleGlobalVariable(const GlobalVariable &G);
    void handleKernel(const Function &F);
    void handleArgument(const Argument &);
    void setAttributesFromMD(const Function &F, oclacc::kernel_p K);

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

    typedef std::unordered_map<const BasicBlock *, std::vector<const Argument *> > StreamAccessMapTy;
    typedef StreamAccessMapTy::iterator StreamAccessMapIt;
    typedef StreamAccessMapTy::const_iterator StreamAccessMapConstIt;

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
    void handleGEPOperator(GEPOperator &);

    void visitCallInst(CallInst &);
    void visitCmpInst(CmpInst &);
    //void visitICmpInst(ICmpInst &);
    //void visitFCmpInst(FCmpInst &);
    void visitPHINode(PHINode &);
    void visitBranchInst(BranchInst &);

  private:
    KernelMapTy KernelMap;
    BlockMapTy BlockMap;
    
    // Normal Instructions and propagated Arguments.
    BlockValueMapTy BlockValueMap;
    
    // Store Kernel Arguments. No mapping for propagated Arguments.
    ArgMapTy ArgMap;

    // 
    StreamAccessMapTy ArgStreamReads;
    StreamAccessMapTy ArgStreamWrites;


    oclacc::DesignUnit HWDesign;

    const DataLayout *DL;

  private:
    oclacc::const_p makeConstant(const Constant *, const Instruction *);

    /// \brief Return run-time operand for struct offset.
    /// \param Parent BasicBlock pointer to add Operation to HWParent
    /// \param IndexValue Constant or Dynamic Index
    /// \param IndexTy Type of the indexed value
    oclacc::base_p computeSequentialIndex(BasicBlock *, Value *IndexValue, SequentialType *IndexTy);

    /// \brief Return run-time operand for struct offset.
    /// \param Parent BasicBlock pointer to add Operation to HWParent
    /// \param IndexValue Constant or Dynamic Index
    /// \param IndexTy Type of the indexed value
    ///
    /// Skip \param IndexValue struct elements of \param IndexTy
    /// The resulting offset is a ConstVal because the type of the struct and
    /// the element access are static.
    oclacc::const_p computeStructIndex(BasicBlock *, Value *IndexValue, StructType *IndexTy);


    /// \brief Create shared_ptr to HW object and return requested pointer type.
    ///
    /// Does not set Parent pointer for the created object.
    template<class HW, class ...Args>
    std::shared_ptr<HW> makeHW(const Value *IR, Args&& ...args) {

      // A single Constant value might be used by
      // multiple instructions, leading to conflicts in the Map.
      assert(! isa<ConstantInt>(IR) && !isa<ConstantFP>(IR) && "Do not use makeHW to create constants.");

      std::shared_ptr<HW> HWP = std::make_shared<HW>(args...);
      HWP->setIR(IR);

      return HWP;
    }

    /// \brief Create shared_ptr to HW object and add to ValueMap for \param BB
    ///
    /// Set Parent pointer for the created object.
    template<class T, class ...Args>
    std::shared_ptr<T> makeHWBB(const BasicBlock *BB, const Value *IR, Args&& ...args) {

      std::shared_ptr<T> HWP = makeHW<T>(IR, args...);
      oclacc::block_p HWBB = getBlock(BB);
      HWP->setParent(HWBB);

      BlockValueMap[BB][IR] = HWP;

      // Only add operations (Instructions) to Blocks.
      //if (!std::is_base_of<oclacc::Port, HW>::value) HWB->addOp(HWP);
      HWBB->addOp(HWP);

      return HWP;
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

    bool existsHW(const BasicBlock *BB, const Value *IR) const {
      BlockValueMapConstIt BI = BlockValueMap.find(BB);

      if (BI == BlockValueMap.end()) return false;

      ValueMapConstIt VI = BI->second.find(IR);

      if (VI == BI->second.end()) return false;

      return true;

    }


    template<class HW>
    std::shared_ptr<HW> getHW(const BasicBlock *BB, const Value *IR) const {

      BlockValueMapConstIt BI = BlockValueMap.find(BB);
      ValueMapConstIt VI;

      if (BI != BlockValueMap.end())
        VI = BI->second.find(IR);
      

      // The block may not exist yet in BlockValueMap though the block itself
      // exists in BlockMap.
      //
      if (BI == BlockValueMap.end() || VI == BI->second.end()) {
        // Real Arguments are valid in every BasicBlock and thus are not listed in
        // the BlockValueMap. Propagated WI functions however should have been
        // added to the BlockValueMap, so we can be sure that we can directly
        // use the Argument port
        //
        if (const Argument *A = dyn_cast<Argument>(IR)) {
          ArgMapConstIt AI = ArgMap.find(A);
          if (AI == ArgMap.end()) {
            IR->dump();
            llvm_unreachable("No Arg");
          }

          return std::static_pointer_cast<HW>(AI->second);
        } 
        else {
          BB->dump();
          IR->dump(); 
          llvm_unreachable("No HW, No Arg");
        }
      } else
        return std::static_pointer_cast<HW>(VI->second);
    }

    /// \brief Return a base class object
    oclacc::base_p getHW(const BasicBlock *BB, const Value *IR) const {
      return getHW<oclacc::HW>(BB, IR);
    }

    /// \brief Return true if Value is already Valid in \param BB
    ///
    bool isValueInBB(const BasicBlock *BB, const Value *IR) const {
      BlockValueMapConstIt BI = BlockValueMap.find(BB);
      ValueMapConstIt VI;

      if (BI != BlockValueMap.end())
        VI = BI->second.find(IR);
      

      // The block may not exist yet in BlockValueMap though the block itself
      // exists in BlockMap.
      //
      if (BI != BlockValueMap.end() && VI != BI->second.end())
        return true;

      return false;
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

    oclacc::Datatype getDatatype(const Type *T) const {
      oclacc::Datatype DT=oclacc::Invalid;
      if (T->isIntegerTy()) DT=oclacc::Integer;
      else if (T->isHalfTy()) DT=oclacc::Half;
      else if (T->isFloatTy()) DT=oclacc::Float;
      else if (T->isDoubleTy()) DT = oclacc::Double;
      else if (T->isStructTy()) DT = oclacc::Struct;
      else {
        T->dump();
        llvm_unreachable("Invalid Type");
      }

      return DT;
    }
};

} //end namespace llvm

#endif /* OCLACCHWPASS_H */
