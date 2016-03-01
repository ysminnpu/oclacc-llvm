#ifndef CREATEBLOCKSPASS_H
#define CREATEBLOCKSPASS_H


#include "llvm/IR/Instruction.h"
#include "llvm/IR/InstVisitor.h"

#include <map>
#include <set>
#include <utility>

#include "Macros.h"
#include "HW/typedefs.h"
#include "HW/Kernel.h"


namespace llvm {

/// \brief Walk through BasicBlocks and add all Values defined in others as
/// output and input.
///
class CreateBlocksPass : public FunctionPass, public InstVisitor<CreateBlocksPass> {
  public:
    static char ID;

    typedef std::vector<oclacc::block_p> BlockListType;
    BlockListType Blocks;

  private:
    bool isDefInCurrentBB(Value *);
    oclacc::block_p createHWBlock(BasicBlock* BB);

    /// FIXME:
    BasicBlock *getSingleSuccessor(BasicBlock *) const;
    BasicBlock *getUniqueSuccessor(BasicBlock *) const;

    BasicBlock *CurrentBB = nullptr;

  public:
    CreateBlocksPass();
    ~CreateBlocksPass();

    NO_COPY_ASSIGN(CreateBlocksPass)

    virtual const char *getPassName() const { return "OCLAcc CreateBlocksPass"; }
    virtual bool doInitialization(Module &);
    virtual bool doFinalization(Module &);
    virtual void getAnalysisUsage(AnalysisUsage &AU) const;
    virtual bool runOnFunction(Function &);

    BlockListType &getBlocks() {
      return Blocks;
    }

    void visitBasicBlock(BasicBlock &);
};

CreateBlocksPass *createCreateBlocksPass();

} //end namespace llvm

#endif /* CREATEBLOCKSPASS_H */
