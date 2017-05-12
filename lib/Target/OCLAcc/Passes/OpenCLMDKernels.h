//===- OpenCLMDKernels.h - Detect OpenCL kernel functions -----------------===//
//
// Detects OpenCL kernel functions by inspecting the "opencl.kernels" metadata
// nodes created by the SPIR 2.0 compiler.
//
//===----------------------------------------------------------------------===//

#ifndef _LOOPUS_OPENCLKERNELS_H_INCLUDE_
#define _LOOPUS_OPENCLKERNELS_H_INCLUDE_

#include "llvm/IR/Metadata.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Constants.h"
#include "llvm/Pass.h"
#include "llvm/Support/raw_ostream.h"

#include <map>
#include <vector>

/// \brief The class stores some bits for each function so they must not be
/// \brief recomputed on every request.
struct KernelFInfoFlags {
  bool isKernelF  : 1;
  bool isWorkItem : 1;

  KernelFInfoFlags()
   : isKernelF(true), isWorkItem(false) {
  }
};

/// \brief The class stores all information gather for one kernel function.
struct KernelFInfo {
  struct KernelFInfoFlags Flags;
  llvm::MDNode *MDN;

  KernelFInfo()
   : MDN(0) {
  }
  KernelFInfo(const struct KernelFInfoFlags &flags)
   : Flags(flags), MDN(0) {
  }
};

/// \brief Detects kernel functions available in a module by inspecting the
/// metadata nodes.
///
/// The pass tries to detect all available OpenCL kernel functions in the
/// processed module by inspecting the "opencl.kernels" metadata node. The SPIR
/// compiler normally creates that metadat node and puts a list of all kernel
/// functions into that node. This pass reads that node provides the list of
/// kernel functions to its users. If an error occured while inspecting the
/// metadata node this is indicated by the ResultIsValid flag.
class OpenCLMDKernels : public llvm::ModulePass {
  private:
    bool foundMDNOpenCLKernels;
    bool foundMDNOclaccWorkitems;
    std::map<llvm::Function*, KernelFInfo> KernelInfo;

  protected:
    bool evaluateOpenCLKernelsMD(const llvm::Module &M);
    bool evaluateOclaccWorkitems(const llvm::Module &M);

  public:
    static char ID;

    OpenCLMDKernels(void);
    virtual void getAnalysisUsage(llvm::AnalysisUsage &AU) const override;
    virtual void print(llvm::raw_ostream &O, const llvm::Module *M) const override;
    virtual bool runOnModule(llvm::Module &M) override;

    void getKernelFunctions(std::vector<llvm::Function*> &list) const;
    void getWorkitemFunctions(std::vector<llvm::Function*> &list) const;
    void getSingleFunctions(std::vector<llvm::Function*> &list) const;
    llvm::MDNode* getMDNodeForFunction(const llvm::Function* const F) const;

    bool isKernel(const llvm::Function* const F) const;
    bool isWorkitemFunction(const llvm::Function* const F) const;
    bool isSingleFunction(const llvm::Function* const F) const;

    unsigned getRequiredWorkGroupSize(const llvm::Function &F, unsigned Dimension);
    const llvm::ConstantInt* getRequiredWorkGroupSizeConst(const llvm::Function &F, unsigned Dimension);
};

#endif

