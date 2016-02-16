#include "llvm/ADT/SmallPtrSet.h"
#include "llvm/ADT/StringExtras.h"
#include "llvm/Analysis/AliasAnalysis.h"
#include "llvm/Analysis/CFG.h"
#include "llvm/Analysis/LoopInfoImpl.h"
#include "llvm/Analysis/Passes.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/Config/config.h"
#include "llvm/IR/CallingConv.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/GlobalValue.h"
#include "llvm/IR/InlineAsm.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/InstVisitor.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Module.h"
#include "llvm/MC/MCAsmInfo.h"
#include "llvm/MC/MCInstrInfo.h"
#include "llvm/MC/MCSubtargetInfo.h"
#include "llvm/PassAnalysisSupport.h"
#include "llvm/Pass.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/FormattedStream.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/TargetRegistry.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <map>
#include <set>
#include <unistd.h>

#include "OCLAccHWPass.h"
#include "CreateBlocksPass.h"
#include "OCLAccHWVisitor.h"
#include "OCLAccTargetMachine.h"
#include "OCLAccGenSubtargetInfo.inc"
#include "OpenCLDefines.h"

#include "HW/HW.h"
#include "HW/typedefs.h"
#include "HW/Design.h"
#include "HW/Visitor/Dot.h"
#include "HW/Visitor/Vhdl.h"
#include "HW/Visitor/Latency.h"

namespace llvm {

using namespace oclacc;

/* 
 * OpenCL mandatory functions
 */
//math intrinsics
static cl::opt<bool> clSinglePrecisionConstant("cl-single-precision-constant", cl::init(false), cl::desc("Treat double precision floating-point constant as single precision constant.") );
static cl::opt<bool> clDenormsAreZero("cl-denorms-are-zero", cl::init(false), cl::desc("Denormalized numbers may be flushed to zero.") );

//optimization
static cl::opt<bool> clOptDisable("cl-opt-disable", cl::init(false), cl::desc("Disables all optimizations.") );
static cl::opt<bool> clStrictAliasing("cl-strict-aliasing", cl::init(false), cl::desc("Assume the strictest aliasing rules.") );
//float options
static cl::opt<bool> clMadEnable("cl-mad-enable", cl::init(false), cl::desc("Allow a * b + c to be replaced by a mad.") );
static cl::opt<bool> clNoSignedZeors("cl-no-signed-zeros", cl::init(false), cl::desc("Ignore the signedness of zero.") );
static cl::opt<bool> clUnsafeMathOperations("cl-unsafe-math-operations", cl::init(false), cl::desc("Includes the -cl-no-signed-zeros and -cl-mad-enable options.") );
static cl::opt<bool> clFiniteMathOnly("cl-finite-math-only", cl::init(false), cl::desc("Assume that arguments and results are not NaNs or ±∞.") );
static cl::opt<bool> clRelaxedMath("cl-relaxed-math", cl::init(false), cl::desc("Sets -cl-finite-math-only and -cl-unsafe-math-optimizations") );

/*
 * OCLAcc Options.
 *
 * Must be here since results of pass are needed. No need to transfer them to
 * oclacc-llc.
 */
static cl::opt<bool> GenerateDot("oclacc-dot", cl::init(true), cl::desc("Output .dot-Graph for each Kernel-Function.") );

OCLAccHWPass::OCLAccHWPass() : ModulePass(OCLAccHWPass::ID) {
  DEBUG_WITH_TYPE("OCLAccHWPass", dbgs() << "OCLAccHWPass created\n");
}

OCLAccHWPass::~OCLAccHWPass() {
}

OCLAccHWPass *OCLAccHWPass::createOCLAccHWPass() {
  return new OCLAccHWPass();
}

bool OCLAccHWPass::doInitialization(Module &M) {
  return false;
}

bool OCLAccHWPass::doFinalization(Module &M) {
  return false;
}

void OCLAccHWPass::getAnalysisUsage(AnalysisUsage &AU) const {
  AU.addRequired<CreateBlocksPass>();
  AU.setPreservesAll();
}

void OCLAccHWPass::createMakefile() {
  std::ofstream F;
  F.open("Makefile", std::ios::out | std::ios::app);
  F << "dot=$(wildcard *.dot)\n";
  F << "png=$(patsubst %.dot,%.png,$(dot))\n";
  F << "svg=$(patsubst %.dot,%.svg,$(dot))\n";
  F << "pdf=$(patsubst %.dot,%.pdf,$(dot))\n";
  F << "all: $(png) $(svg) $(pdf)\n";
  F << "%.png: %.dot\n";
  F << "\tdot -Tpng $< > $@\n";
  F << "%.svg: %.dot\n";
  F << "\tdot -Tsvg $< > $@\n";
  F << "%.pdf: %.svg\n";
  F << "\tinkscape -D $< -A $@\n";
  F.close();
}

/// \brief Main HW-Generation Pass
/// This Pass defines the dependencies to Transform- and Analysis Passes to
/// generate a HW Design from a given LLVM-IR (SPIR) code.
///
/// Expensive Transformations which eventually run in an optimization
/// loop, e.g. loop unrolling or vectorization, have to be performed earlier. 
///
bool OCLAccHWPass::runOnModule(Module &M) {

  StringRef ModuleName = M.getName();

  outs() << "Compile Module " << ModuleName << "\n";

  createMakefile();

  //Global variables must be __constant
  for ( GlobalVariable &G : M.getGlobalList()) {
    PointerType *GT = G.getType();

    if ( GT->getAddressSpace() !=  ocl::AS_CONSTANT )
      llvm_unreachable( "Global Variable is not constant" );
  }

  //FIXME: unused debug info
  NamedMDNode *debug = M.getNamedMetadata("llvm.dbg.cu");
  if ( ! debug ) {
    outs() << "No debug information for Module " << M.getName() << "\n";
  }

  Design.setName(ModuleName);

  // Process all kernel functions
  for (Function &KernelFunction: M.getFunctionList()) {

    // Omit declarations, e.g. to built-in functions.
    if (KernelFunction.isDeclaration()) 
      continue;

    bool isWorkItemKernel=false;
    std::string KernelName = KernelFunction.getName();
    oclacc::kernel_p HWKernel = oclacc::makeHW<oclacc::Kernel>(&KernelFunction, KernelName, isWorkItemKernel);

    //Visit all Arguments first
    for (const Argument &Arg : KernelFunction.getArgumentList()) {
      handleArgument(HWKernel, Arg);
    }

    // We directly generate the control flow through the basic block.
    CreateBlocksPass &CBP = getAnalysis<CreateBlocksPass>(KernelFunction);
    CreateBlocksPass::BlockListType HWBlocks = CBP.getBlocks();

#if 0
    // Separatly visit each Basic Block
    for (BasicBlock &BB : KernelFunction.getBasicBlockList()) {
      V.visit(BB);
    }
#endif

#if 0
    // Instantiate visitors for each Function to create HW hierarchy
    OCLAccHWVisitor V(HWKernel);

    Design.addKernel(V.getKernel());
#endif
  }

  if (GenerateDot) {
    DotVisitor V;
    Design.accept(V);
  }

  return false;
}

/// \brief Create ScalarInput/InputStream for Kernel
///
/// Depending on its type, InScalar (int, float) or Streams referencing 
/// global memory references are created and assigned to the kernel.
///
void OCLAccHWPass::handleArgument(oclacc::kernel_p HWKernel, const Argument &A) {
  Type *AType = A.getType();
  std::string Name = A.getName().str();

  if ( AType->isIntegerTy() ) {
    inscalar_p Scalar = makeHW<InScalar>(&A, Name, AType->getScalarSizeInBits());
    HWKernel->addInScalar(Scalar);
    errs() << Name << " is InScalar (TODO int) of size " << AType->getScalarSizeInBits() << "\n";
  } else if ( AType->isFloatingPointTy() ) {
    inscalar_p Scalar = makeHW<InScalar>(&A, Name, AType->getScalarSizeInBits());
    HWKernel->addInScalar(Scalar);
    errs() << Name << " is InScalar (TODO float) of size " << AType->getScalarSizeInBits() << "\n";
  } else if (AType->isPointerTy()) {
    bool isRead = false;
    bool isWritten = false;

    if ( A.onlyReadsMemory() ) {
      isRead = true;
    } else {
      // have to check the whole hierarchy
      std::list<const Value *> Values;
      for ( auto &Inst : A.uses() ) {                                
        Values.push_back(Inst.getUser());                                   
      } 

      while (! Values.empty()) {
        const Value *CurrVal = Values.back();
        Values.pop_back();

        if (const StoreInst *I = dyn_cast<StoreInst>(CurrVal)) {
          isWritten = true;
        } else if (const LoadInst *I = dyn_cast<LoadInst>(CurrVal)) {
          isRead = true;
        } else if (const GetElementPtrInst *I = dyn_cast<GetElementPtrInst>(CurrVal)) {
          for ( auto &Inst : CurrVal->uses() ) {
            Values.push_back(Inst.getUser());
          }
        } else {
          CurrVal->dump();
          llvm_unreachable("Invalid use of ptr operand.");
        }
      }
    }

    errs() << A.getName() << (isRead ? " r" : " ") << (isWritten ? "w\n" : "\n");

    Type *ElementType   = AType->getPointerElementType();
    unsigned SizeInBits = ElementType->getScalarSizeInBits();

    unsigned AddressSpace = AType->getPointerAddressSpace();

    switch ( AddressSpace ) {
      case ocl::AS_LOCAL:
        llvm_unreachable("Local arguments not implemented.");
      case ocl::AS_GLOBAL:
        if (isWritten) {
          outstream_p S = makeHW<OutStream>(&A, Name, SizeInBits);
          HWKernel->addOutStream(S);
          errs() << Name << " is InStream of size " <<  SizeInBits << "\n";
        } else if (isRead ) {
          instream_p S = makeHW<InStream>(&A, Name, SizeInBits);
          HWKernel->addInStream(S);
          //ValueMap[&A] = Stream;
          errs() << Name << " is OutStream of size " <<  SizeInBits << "\n";
        } else {
          llvm_unreachable("Stream neither read nor written.");
        }
        break;
      default:
        errs() << "AddressSpace of "<< A.getName() << ": " << AddressSpace << "\n";
        llvm_unreachable( "AddressSpace not supported" );
    }
  }  else {
    llvm_unreachable("Unknown Argument Type");
  }
}

char OCLAccHWPass::ID = 0;

static RegisterPass<OCLAccHWPass> X("oclacc-hw", 
    "Create HW Tree.");

} // end namespace llvm


