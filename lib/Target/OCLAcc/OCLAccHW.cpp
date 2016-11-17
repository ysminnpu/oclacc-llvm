#include "llvm/ADT/SmallPtrSet.h"
#include "llvm/ADT/StringExtras.h"
#include "llvm/ADT/SmallSet.h"
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
#include "llvm/IR/Instructions.h"
#include "llvm/IR/InstVisitor.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/PassManager.h"
#include "llvm/IR/IRPrintingPasses.h"
#include "llvm/MC/MCAsmInfo.h"
#include "llvm/MC/MCInstrInfo.h"
#include "llvm/MC/MCSubtargetInfo.h"
#include "llvm/PassAnalysisSupport.h"
#include "llvm/Pass.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/FormattedStream.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/TargetRegistry.h"

#include "llvm/Transforms/Loopus.h"
#include "../../../lib/Transforms/Loopus/OpenCLMDKernels.h"
#include "../../../lib/Transforms/Loopus/HDLPromoteID.h"
#include "../../../lib/Transforms/Loopus/ArgPromotionTracker.h"
#include "../../../lib/Transforms/Loopus/BitWidthAnalysis.h"
#include "../../../lib/Transforms/Loopus/FindAllPaths.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <map>
#include <list>
#include <set>
#include <fstream>
#include <sstream>
#include <unistd.h>

#include "macros.h"
#include "OCLAccHW.h"
#include "OCLAccTargetMachine.h"
#include "OCLAccGenSubtargetInfo.inc"
#include "OpenCLDefines.h"

#include "HW/HW.h"
#include "HW/Arith.h"
#include "HW/typedefs.h"
#include "HW/Design.h"
#include "HW/Streams.h"
#include "HW/Kernel.h"
#include "HW/Constant.h"
#include "HW/Compare.h"
#include "HW/Control.h"

#include <cxxabi.h>
#define TYPENAME(x) abi::__cxa_demangle(typeid(x).name(),0,0,NULL)

#define DEBUG_TYPE "oclacchw"

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

INITIALIZE_PASS_BEGIN(OCLAccHW, "oclacc-hw", "Generate OCLAccHW",  false, true)
INITIALIZE_PASS_DEPENDENCY(HDLPromoteID);
INITIALIZE_PASS_DEPENDENCY(ArgPromotionTracker);
INITIALIZE_PASS_DEPENDENCY(OpenCLMDKernels);
INITIALIZE_PASS_DEPENDENCY(BitWidthAnalysis);
INITIALIZE_PASS_DEPENDENCY(FindAllPaths);
INITIALIZE_PASS_END(OCLAccHW, "oclacc-hw", "Generate OCLAccHW",  false, true)

char OCLAccHW::ID = 0;

namespace llvm {
  ModulePass *createOCLAccHWPass() { 
    return new OCLAccHW(); 
  }
}

OCLAccHW::OCLAccHW() : ModulePass(OCLAccHW::ID) {
  DEBUG(dbgs() << "OCLAccHW created\n");
}

OCLAccHW::~OCLAccHW() {
}

bool OCLAccHW::doInitialization(Module &M) {
  return false;
}

bool OCLAccHW::doFinalization(Module &M) {
  return false;
}

void OCLAccHW::getAnalysisUsage(AnalysisUsage &AU) const {
  AU.addRequired<HDLPromoteID>();
  AU.addRequired<ArgPromotionTracker>();
  AU.addRequired<OpenCLMDKernels>();
  AU.addRequired<BitWidthAnalysis>();
  AU.addRequired<FindAllPaths>();
}

void OCLAccHW::createMakefile() {
  std::ofstream F;
  F.open("Makefile", std::ios::out | std::ios::trunc);
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


/// \brief Main HW generation Pass
///
bool OCLAccHW::runOnModule(Module &M) {

  const std::string ModuleName = M.getName();

  createMakefile();

  // OpenCL-C 2.0 6.5
  // __global or __constant
  for (const GlobalVariable &G : M.getGlobalList()) {
    PointerType *GT = G.getType();
    const unsigned AS = GT->getAddressSpace();

    if (AS != ocl::AS_CONSTANT && AS != ocl::AS_GLOBAL)
      OCL_ERR("Program scope variable not constant or global");
  }

  //FIXME: unused debug info
  NamedMDNode *debug = M.getNamedMetadata("llvm.dbg.cu");
  if (! debug) {
    outs() << "No debug information for Module " << M.getName() << "\n";
  }

  HWDesign.setName(ModuleName);

  // Print current state of optimization
  std::error_code EC;
  std::string FileName = ModuleName+".oclacchw.ll";
  raw_fd_ostream File(FileName, EC, llvm::sys::fs::F_RW | llvm::sys::fs::F_Text);
  if (EC) {
    errs() << "Failed to create " << FileName << "(" << __LINE__ << "): " << EC.message() << "\n";
    return -1;
  }
  PrintModulePass PrintPass(File);
  PrintPass.run(M);

  // Process all kernel functions
  OpenCLMDKernels &CLK = getAnalysis<OpenCLMDKernels>();
  std::vector<Function*> Kernels;
  CLK.getKernelFunctions(Kernels);

  for (Function *KF: Kernels) {
    // We do currently not support loops.
    SmallVector<std::pair<const BasicBlock*,const BasicBlock*>, 32 > Result;
	  FindFunctionBackedges(*KF, Result);
    if (! Result.empty()) {
      TODO("Handle Loops");
      llvm_unreachable("Stop.");
    }

    handleKernel(*KF);

    FindAllPaths &AP = getAnalysis<FindAllPaths>(*KF);
    AP.dump();
  }

  return false;
}

/// \brief Create arguments and basicBlocks for each kernel function
///
void OCLAccHW::handleKernel(const Function &F) {
  OpenCLMDKernels &CLK = getAnalysis<OpenCLMDKernels>();

  bool isWorkItemKernel=CLK.isWorkitemFunction(&F);

  const std::string KernelName = F.getName();

  if (isWorkItemKernel)
    DEBUG(dbgs() << "WorkItemKernel '" << KernelName << "'\n");
  else
    DEBUG(dbgs() << "TaskKernel '" << KernelName << "'\n");

  kernel_p HWKernel = makeKernel(&F, KernelName, isWorkItemKernel);
  HWDesign.addKernel(HWKernel);

  // We first create blocks for each BasicBlock, so we can handle the Kernel
  // Arguments.
  for (const BasicBlock &BB : F.getBasicBlockList()) {
    block_p HWBB = makeBlock(&BB,BB.getName());
  }

  // Create Arguments, assign them to the Kernel and the first BasicBlock.
  for (const Argument &Arg : F.getArgumentList()) {
    handleArgument(Arg);
  }

  // Generate all Ports needed by the BasicBlock and visit all containing
  // Instructions.
  for (const BasicBlock &BB : F.getBasicBlockList()) {
    visit(const_cast<BasicBlock&>(BB));
  }

  HWKernel->dump();

  // Print all Ports of all Blocks.
  for (const BasicBlock &BB : F.getBasicBlockList()) {
    block_p HWB = getBlock(&BB);
    HWB->dump();
  }

}

/// \brief Walk through BBs, set inputs and outputs
///
/// Walk through all instructions, visit their uses and check if they were
/// defined in that BB
void OCLAccHW::visitBasicBlock(BasicBlock &BB) {
  FindAllPaths &AP = getAnalysis<FindAllPaths>(*(BB.getParent()));

  block_p HWBB = getBlock(&BB);

  std::vector<const Argument *> AS;
  std::vector<const Instruction *> VS;

  // Collect all foreign Instructions and Arguments used by this BasicBlock
  // to add them as Ports.
  for (const Instruction &I : BB.getInstList()) {
    for (const Use &U : I.operands()) {
      const Value *V = U.get();

      // Do we already have a Mapping Value->HW valid in this BB?
      if (existsHW(&BB, V)) 
        continue;

      // Constants are created when they are used, BasicBlocks are PHINode
      // Operands and can be skipped.
      //
      if (isa<Constant>(V) || isa<BasicBlock>(V)) 
        continue;

      // We handle Arguments and Instructions separately. Instructions can be
      // safely connected as their definition has already been visited.
      //
      if (const Instruction *I = dyn_cast<Instruction>(V)) {

        // Only handle Instructions not defined in BB
        if (I->getParent() == &BB)
          continue;

        VS.push_back(I); 
      } else if (const Argument *A = dyn_cast<Argument>(V))
        AS.push_back(A);
      else {
        V->dump();
        llvm_unreachable("unknown value");
      }
    }
  }

  // Multiple uses of a single Value should result in a single Port. So we
  // eliminate duplicates.
  std::sort(VS.begin(), VS.end());
  VS.erase(std::unique(VS.begin(), VS.end()), VS.end());

  std::sort(AS.begin(), AS.end());
  AS.erase(std::unique(AS.begin(), AS.end()), AS.end());

  for (const Instruction *I : VS) {
    // The current block BB uses a value defined in a different block. We have
    // to:
    // - find the complete path from the definition's block to the current
    // - between each two blocks, create a single ScalarPort SP to connect both
    // - add SP as OutScalar in DefBB
    // - add SP as InScalar in BB
    // - connect the definition with the OutScalar
    // - connect the user with the InScalar
    // - create a blockValueMap entry BB->(I->ScalarPort) for each newly created
    // Port
    //
    // We can be sure that all blocks from definition to use exist because only
    // values defined before in the source file can be used as operand.

    const BasicBlock *DefBB = I->getParent();

    block_p HWDefBB = getBlock(DefBB);
    base_p HWI = getHW<HW>(DefBB, I);

    // The type will be the same for all newly created ports
    Type *IT = I->getType();
    const std::string Name = I->getName();

    const FindAllPaths::PathTy &Paths = AP.getPathFromTo(DefBB, &BB);

    if (IT->isIntegerTy() || IT->isFloatingPointTy()) { 
      for (const FindAllPaths::SinglePathTy P : Paths) {

        for (FindAllPaths::SinglePathConstIt FromBBIt = P.begin(), ToBBIt = std::next(FromBBIt); 
            ToBBIt != P.end(); 
            FromBBIt = ToBBIt, ++ToBBIt ) 
        {
          block_p HWFrom = getBlock(*FromBBIt);
          block_p HWTo = getBlock(*ToBBIt);

          scalarport_p HWOut;
          scalarport_p HWIn;

          if (!HWFrom->containsOutScalarForValue(I)) {
            HWOut = makeHWBB<ScalarPort>(*FromBBIt, I, I->getName(), IT->getScalarSizeInBits(), getDatatype(IT));
            HWFrom->addOutScalar(HWOut);
            connect(HWI, HWOut);
          } else
            HWOut = HWFrom->getOutScalarForValue(I);

          if (!HWTo->containsInScalarForValue(I)) {
            HWIn = makeHWBB<ScalarPort>(*ToBBIt, I, I->getName(), IT->getScalarSizeInBits(), getDatatype(IT));
            HWTo->addInScalar(HWOut);
          } else
            HWIn = HWTo->getInScalarForValue(I);

          assert(HWOut != nullptr && HWIn != nullptr);

          // TODO: use BitWidthAnalysis

          connect(HWOut, HWIn);

          // Proceed HWI to connect potential next blocks port to
          HWI = HWIn;
        }
      }
    } 
    else if (IT->isPointerTy()) {
      IT->dump();
      llvm_unreachable("pointer operand in code. fixme.");
    }
  }

  ArgPromotionTracker &AT = getAnalysis<ArgPromotionTracker>();

  // Arguments have to be handles twofold. Propagated WI-Arguments have to be
  // passed from BB to BB, real arguments are static for all BBs.
  //
  const BasicBlock *EntryBB = &(BB.getParent()->getEntryBlock());

  for (const Argument *A : AS) {
    // Only WI functions have to be passed from block to block. Other Arguments
    // are static and can be used directly. There must exist a ScalarPort for
    // the Argument as they are created before visiting the BasicBlocks.
    //
    if (AT.isPromotedArgument(A)) {
      base_p HWI = getHW<ScalarPort>(&BB, A);
      const Type *AType = A->getType();
      
      if (!AType->isIntegerTy()) {
        A->dump();
        AType->dump();
        llvm_unreachable("stop");
      }

      const FindAllPaths::PathTy &Paths = AP.getPathFromTo(EntryBB, &BB);

      for (const FindAllPaths::SinglePathTy P : Paths) {

        for (FindAllPaths::SinglePathConstIt FromBBIt = P.begin(), ToBBIt = std::next(FromBBIt); 
            ToBBIt != P.end(); 
            FromBBIt = ToBBIt, ++ToBBIt ) 
        {
          block_p HWFrom = getBlock(*FromBBIt);
          block_p HWTo = getBlock(*ToBBIt);
          // TODO: use BitWidthAnalysis

          scalarport_p HWP = makeHWBB<ScalarPort>(&BB, A, A->getName(), AType->getScalarSizeInBits(), getDatatype(AType));
          connect(HWI, HWP);

          // Add Port to defining and using Block
          HWFrom->addOutScalar(HWP);
          HWTo->addInScalar(HWP);

          // Proceed HWI to connect potential next blocks port to
          HWI = HWP;
        }
      }
    } else {
      // Nothing has to be done right now. We will connect the Argument with its
      // user when the user is visited.
    }
  }
}
#if 0
      Type *ElementType = IT->getPointerElementType();
      //TODO Use Bitwidth from analysis
      unsigned Bits = ElementType->getScalarSizeInBits();

      // Check Address Space
      unsigned LAS = ElementType->getPointerAddressSpace();
      ocl::AddressSpace AS = static_cast<ocl::AddressSpace>(LAS);

      switch (AS) {
        case ocl::AS_GLOBAL:
          break;
        case ocl::AS_LOCAL:
          break;
        default:
          llvm_unreachable("Invalid AddressSpace");
      }

    // Find all possible paths from DefBB to BB
    for (const FindAllPaths::SinglePathTy P : AP.getPathFromTo(DefBB, &BB)) {

      for (FindAllPaths::SinglePathConstIt FromBBIt = P.begin(), ToBBIt = std::next(FromBBIt); 
          ToBBIt != P.end(); 
          FromBBIt = ToBBIt, ++ToBBIt ) 
      {
        // Do we have a Scalar or StreamPort?
        if (VT->isIntegerTy() || VT->isFloatingPointTy()) { 
          scalarport_p HWP = makeHWBB<ScalarPort>(&BB, V, V->getName(), VT->getScalarSizeInBits(), getDatatype(VT));
          connect(HWI, HWP);

          // Proceed HWI to connect potential next blocks port to
          HWI = HWP;

          // Add Port to defining and using Block
          HWDef->addOutScalar(HWP);
          HWBB->addInScalar(HWP);
        } 
        else if (VT->isPointerTy()) {
          Type *ElementType   = VT->getPointerElementType();
          //TODO Use Bitwidth from analysis
          unsigned Bits = ElementType->getScalarSizeInBits();

          // Check Address Space
          unsigned LAS = ElementType->getPointerAddressSpace();
          ocl::AddressSpace AS = static_cast<ocl::AddressSpace>(LAS);

          switch (AS) {
            case ocl::AS_GLOBAL:
              break;
            case ocl::AS_LOCAL:
              break;
            default:
              llvm_unreachable("Invalid AddressSpace");
          }

          const Datatype D = getDatatype(ElementType);
          streamport_p HWS = makeHWBB<StreamPort>(&BB, V, Name, Bits, AS, D);

          connect(HWI, HWS);

          HWDef->addOutStream(HWS);
          HWBB->addInStream(HWS);
        } else 
          llvm_unreachable("stop");
      }
    }
  }
}
#endif

/// \brief Create ScalarInput/InputStream for Kernel
///
/// Depending on its type, Scalar (int, float) or Streams referencing
/// global memory are created and assigned to the kernel. Input and output is
/// split, so a single argument may result in two ports.
///
/// Arguments are either actual values directly passed to the kernel or
/// references to memory.
///
/// Argument types are later handled differently:
///  - Actual scalars: same for all WIs, no propagation between BBs
///  - Eliminated WI functions: propagate them from block to block.
///  - Streams: same for all WIs
///
/// The separation between WI and static scalars is respected when used. Their 
/// generation only depends on their type.
///
/// OpenCL 2.0 pp 34: allowed address space for variables is _private,
/// pointers may be global, local or constant
void OCLAccHW::handleArgument(const Argument &A) {
  const Type *AType = A.getType();
  const std::string Name = A.getName();

  const Function *F = A.getParent();

  kernel_p HWKernel = getKernel(F);

  const BasicBlock *EB = &(F->getEntryBlock());
  block_p HWEB = getBlock(EB);

  // TODO Use BitWdith Analysis
  BitWidthAnalysis &BW = getAnalysis<BitWidthAnalysis>(const_cast<Function &>(*F));
  std::pair<int, Loopus::ExtKind> W = BW.getBitWidth(&A);
  unsigned Bits = W.first;

  errs() << "Argument " << A.getName() << " uses " << Bits << " Bits\n";

  if (AType->isIntegerTy() || AType->isFloatingPointTy()) {
    scalarport_p S = makeHW<ScalarPort>(&A, Name, AType->getScalarSizeInBits(), getDatatype(AType));
    HWKernel->addInScalar(S);
    ArgMap[&A] = S;
  } 
  else if (const PointerType *PType = dyn_cast<PointerType>(AType)) {
    bool isRead = false;
    bool isWritten = false;

    unsigned AS = PType->getAddressSpace();

    switch (AS) {
      case ocl::AS_GLOBAL:
        break;
      case ocl::AS_LOCAL:
        break;
      case ocl::AS_CONSTANT:
        break;
      default:
        llvm_unreachable("Invalid AddressSpace");
    }

    // Walk through use list and collect Load/Store/GEP Instructions using it
    std::list<const Value *> Values;
    for ( const auto &Inst : A.uses() ) {
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

    const Type *ElementType   = AType->getPointerElementType();

    if (!isWritten && !isRead)
      DEBUG(dbgs() << "omitting unused Argument " << A.getName() << "\n");
    else {
      const Datatype D = getDatatype(ElementType);
      streamport_p S = makeHW<StreamPort>(&A, Name, Bits, static_cast<ocl::AddressSpace>(AS), D);
      ArgMap[&A] = S;

      if (isWritten) {
        HWKernel->addOutStream(S);
        DEBUG(dbgs() << "Argument '" << Name << "' is OutStream (" << Strings_Datatype[D] << ":" <<  Bits << ")\n");
      }
      if (isRead) {
        HWKernel->addInStream(S);
        DEBUG(dbgs() << "Argument '" << Name << "' is InStream (" << Strings_Datatype[D] << ":" <<  Bits << ")\n");
      }
    }
  }
  else
    llvm_unreachable("Unknown Argument Type");
}

//////////////////////////////////////////////////////////////////////////////
// Visit Functions
//////////////////////////////////////////////////////////////////////////////

void OCLAccHW::visitBinaryOperator(BinaryOperator &I) {
  Value *IVal = &I;
  Type *IType = I.getType();
  const BasicBlock *BB = I.getParent();

  errs() << I.getName() << " in block " << I.getParent()->getName() << "\n";

  std::string IName = I.getName();

  Function *F = I.getParent()->getParent();

  base_p HWOp;

  if (IType->isVectorTy())
    TODO("Implent vector types");

  if (IType->isFloatingPointTy()) {
    unsigned E=0;
    unsigned M=0;

    if (IType->isHalfTy()) {
      M=10;
      E=5;
    } else if (IType->isFloatTy()) {
      M=23;
      E=8;
    } else if (IType->isDoubleTy()) {
      M=52;
      E=11;
    } else if (IType->isFP128Ty()) {
      M=112;
      E=15;
    } else {
      I.dump();
      llvm_unreachable("Invalid FP Type");
    }


    switch (I.getOpcode()) {
      case Instruction::FAdd:
        HWOp = makeHWBB<FAdd>(BB, IVal, IName, M, E);
        break;
      case Instruction::FMul:
        HWOp = makeHWBB<FMul>(BB, IVal,IName, M, E);
        break;
      case Instruction::FRem:
        HWOp = makeHWBB<FRem>(BB, IVal,IName, M, E);
        break;
      case Instruction::FSub:
        HWOp = makeHWBB<FSub>(BB, IVal,IName, M, E);
        break;
      case Instruction::FDiv:
        HWOp = makeHWBB<FDiv>(BB, IVal,IName, M, E);
        break;
    }
  } else {

    BitWidthAnalysis &BW = getAnalysis<BitWidthAnalysis>(*F);
    std::pair<int, Loopus::ExtKind> W = BW.getBitWidth(&I);
    unsigned Bits = W.first;

    // It depends on the Instruction if values have to be interpreted as signed or
    // unsigned.
    switch ( I.getOpcode() ) {
      case Instruction::Add:
        HWOp = makeHWBB<Add>(BB, IVal, IName,Bits);
        break;
      case Instruction::Sub:
        HWOp = makeHWBB<Sub>(BB, IVal,IName,Bits);
        break;
      case Instruction::Mul:
        HWOp = makeHWBB<Mul>(BB, IVal,IName,Bits);
        break;
      case Instruction::UDiv:
        HWOp = makeHWBB<UDiv>(BB, IVal,IName,Bits);
        break;
      case Instruction::SDiv:
        HWOp = makeHWBB<SDiv>(BB, IVal,IName,Bits);
        break;
      case Instruction::URem:
        HWOp = makeHWBB<URem>(BB, IVal,IName,Bits);
        break;
      case Instruction::SRem:
        HWOp = makeHWBB<SRem>(BB, IVal,IName,Bits);
        break;
        //Logical
      case Instruction::Shl:
        HWOp = makeHWBB<Shl>(BB, IVal,IName,Bits);
        break;
      case Instruction::LShr:
        HWOp = makeHWBB<LShr>(BB, IVal,IName,Bits);
        break;
      case Instruction::AShr:
        HWOp = makeHWBB<AShr>(BB, IVal,IName,Bits);
        break;
      case Instruction::And:
        HWOp = makeHWBB<And>(BB, IVal,IName,Bits);
        break;
      case Instruction::Or:
        HWOp = makeHWBB<Or>(BB, IVal,IName,Bits);
        break;
      case Instruction::Xor:
        HWOp = makeHWBB<Xor>(BB, IVal,IName,Bits);
        break;

      default:
        I.dump();
        llvm_unreachable("Unknown Binary Operator");
    }
  }

  for (Value *OpVal : I.operand_values() ) {
    if (Constant *ConstValue = dyn_cast<Constant>(OpVal)) {
      const_p HWConst = makeConstant(ConstValue, &I);

      connect(HWConst,HWOp);
    } else {
      base_p HWOperand = getHW<HW>(I.getParent(), OpVal);

      connect(HWOperand,HWOp);
    }
  }
}

void OCLAccHW::visitLoadInst(LoadInst  &I)
{
  const std::string Name = I.getName().str();
  const Value *AddrVal = I.getPointerOperand();

  unsigned AddrSpace = I.getPointerAddressSpace();

  if ( AddrSpace != ocl::AS_GLOBAL && AddrSpace != ocl::AS_LOCAL )
    llvm_unreachable("NOT_IMPLEMENTED: Only global and local address space supported.");

  //Get Address to load from
  const GetElementPtrInst * P = dyn_cast<GetElementPtrInst>(AddrVal);
  if (!P)
    llvm_unreachable("stop");

  streamindex_p HWStreamIndex = getHW<StreamIndex>(I.getParent(), P);
  if (!HWStreamIndex) {
    llvm_unreachable("Index base address only streams.");
  }

  // get the stream to read from
  streamport_p HWStream = HWStreamIndex->getStream();
  //TODO This is the case for local arrays!
  if (! HWStream)
    llvm_unreachable("Load base address is not a stream");

  HWStream->addLoad(HWStreamIndex);

  BlockValueMap[I.getParent()][&I] = HWStreamIndex;

  HWStreamIndex->setName(HWStream->getName()+"_Index");
}

///
/// \brief Store Instructions perform a write access to memory. Multiple stores
/// to the same memory must preserve their order!
///
/// - Check Namespaces
/// - Address may be constant or Values
/// - If Value: Could be getElementPtrInst
/// - If getElementPtrInst: Connect Base and Index
/// 
void OCLAccHW::visitStoreInst(StoreInst &I)
{
  const std::string Name = I.getName();

  const Value *DataVal = I.getValueOperand();
  const Value *AddrVal = I.getPointerOperand();

  // Check Address Space
  unsigned AS = I.getPointerAddressSpace();

  switch (AS) {
    case ocl::AS_GLOBAL:
      break;
    case ocl::AS_LOCAL:
      break;
    default:
      llvm_unreachable("Invalid AddressSpace");
  }

  base_p HWData;
  base_p HWOut;

  //Get Data to store
  
  if (const Constant *ConstValue = dyn_cast<Constant>(DataVal) ) {
    const_p HWConst = makeConstant(ConstValue, &I);
    HWData = HWConst;
  } else {
    HWData = getHW<HW>(I.getParent(), DataVal);
  }

  //Get Address to store at
  HWOut = getHW<HW>(I.getParent(), AddrVal);

  streamport_p HWStream;
  streamindex_p HWStreamIndex;

  if ((HWStreamIndex = std::dynamic_pointer_cast<StreamIndex>(HWOut))) {
    HWStream = HWStreamIndex->getStream();
  } 
  else if ((HWStream = std::dynamic_pointer_cast<StreamPort>(HWOut))) {
    HWStreamIndex = std::make_shared<StaticStreamIndex>("0", HWStream, 0, 1);
    HWStreamIndex->addOut(HWStream);
  } 
  else {
    llvm_unreachable("Index base address only streams.");
  }

  HWStream->addStore(HWStreamIndex);

  connect(HWData, HWStreamIndex);
}

/// \brief Create StaticStreamIndex or DynamicStreamIndex used by Load or
/// StoreInst.
///
/// By now, we do not know how this stream port is used (load and/or store
/// index). The index used is a separate member of these classes while the
/// load/store connects the index with re stream, either with a value used as
/// input (st) or an output (ld).
///
void OCLAccHW::visitGetElementPtrInst(GetElementPtrInst &I)
{
  Value *InstValue = &I;
  Value *BaseValue = I.getPointerOperand();

  if (! I.isInBounds() )
    llvm_unreachable("Not in Bounds.");

  const std::string Name = I.getName();

  // The pointer base can either be a local Array or a input stream
  unsigned BaseAddressSpace = I.getPointerAddressSpace();
  if ( BaseAddressSpace != ocl::AS_GLOBAL && BaseAddressSpace != ocl::AS_LOCAL )
    llvm_unreachable( "Only global and local address space supported." );

  streamport_p HWBase = getHW<StreamPort>(I.getParent(), BaseValue);

  // Handle index on the base address
  if ( I.getNumIndices() != 1 )
    llvm_unreachable("Only 1D arrays supported");

  Value *IndexValue = *(I.idx_begin());

  base_p HWIndex;
  streamindex_p HWStreamIndex;

  // Create Constant or look for computed Index
  
  if (const Constant *ConstValue = dyn_cast<Constant>(IndexValue)) {
    const Type *CType = ConstValue->getType();

    assert(!CType->isIntegerTy() && "Array Index must be Integer");

    const ConstantInt *IntConst = dyn_cast<ConstantInt>(ConstValue);
    if (!IntConst)
      llvm_unreachable("Unknown Constant Type");

    int64_t C = IntConst->getSExtValue();
    HWStreamIndex = makeHWBB<StaticStreamIndex>(I.getParent(), &I, Name, HWBase, C, IntConst->getValue().getActiveBits());
  } else {
    HWIndex = getHW<HW>(I.getParent(), IndexValue);

    // No need to add StreamIndex to BlockValueMap so create object directly
    HWStreamIndex = makeHWBB<DynamicStreamIndex>(I.getParent(), &I, Name, HWBase, HWIndex);

    HWIndex->addOut(HWStreamIndex);
  }

  BlockValueMap[I.getParent()][InstValue] = HWStreamIndex;

  return;
}

/// \brief Create a Constant. Use bitwidth and datatype from BitWidthAnalysis
///
/// Do not use makeHW to create Constants. A single Constant objects might be used by
/// multiple instructions, leading to conflicts in the ValueMap.
///
const_p OCLAccHW::makeConstant(const Constant *C, const Instruction *I) {
  const BasicBlock * BB = I->getParent();
  block_p HWBlock = getBlock(BB);
  const Function *F = BB->getParent();

  BitWidthAnalysis &BWA = getAnalysis<BitWidthAnalysis>(const_cast<Function &>(*F));
  Loopus::BitWidthRetTy BW = BWA.getBitWidth(C, I);

  std::stringstream CName;
  const_p HWConst;

  const Type *CType = C->getType();

  int Bits = BW.first;

  if (const ConstantInt *IConst = dyn_cast<ConstantInt>(C)) {
    switch (BW.second) {
      case Loopus::SExt:
        {
          int64_t S = IConst->getSExtValue();
          CName << S;
          HWConst = std::make_shared<ConstVal>(CName.str(), S, Bits);
          break;
        }
      case Loopus::ZExt:
        {
          uint64_t U = IConst->getZExtValue();
          CName << U;
          HWConst = std::make_shared<ConstVal>(CName.str(), U, Bits);
          break;
        }
      case Loopus::OneExt:
        {
          TODO("makeConstant OneExt");
          int64_t S = IConst->getSExtValue();
          CName << S;
          HWConst = std::make_shared<ConstVal>(CName.str(), S, Bits);
          break;
        }
      default:
        IConst->dump();
        llvm_unreachable("Invalid sign extention type");
    }
  } 
  else if (const ConstantFP *FConst = dyn_cast<ConstantFP>(C)) {
    assert(BW.second != Loopus::FPNoExt && "Constant is FP Type but FPNoExt is not set");

    const APFloat &Float = FConst->getValueAPF();

    if (CType->isHalfTy()) 
      llvm_unreachable("Half floating point type not supported");
    else if (CType->isFloatTy()) {
      float F = Float.convertToFloat();
      CName << F;
      const APInt Bits = Float.bitcastToAPInt();
      uint64_t V = *(Bits.getRawData());
      HWConst = std::make_shared<ConstVal>(CName.str(), V, Bits.getBitWidth());

    } else if (CType->isDoubleTy()) {
      double F = Float.convertToDouble();
      CName << F;
      const APInt Bits = Float.bitcastToAPInt();
      uint64_t V = *(Bits.getRawData());
      HWConst = std::make_shared<ConstVal>(CName.str(), V, Bits.getBitWidth());
    } else
      llvm_unreachable("Unknown floating point type");
  } else 
  {
    llvm_unreachable("Unsupported Constant Type");
  }

  HWConst->setParent(HWBlock);
  HWBlock->addConstVal(HWConst);

  return HWConst;
}

/// \brief Check if the call is really a built-in
void OCLAccHW::visitCallInst(CallInst &I) {
  const std::string IN = I.getName();
  const Value *Callee = I.getCalledValue();
  std::string CN = Callee->getName().str();

  if (ocl::isArithmeticBuiltIn(CN)) {
    
  } else if (ocl::isWorkItemBuiltIn(CN)) {
    llvm_unreachable("Run pass to inline WorkItem builtins");

  } else {
    errs() << "Function Name: " << CN << "\n";
    llvm_unreachable("Invalid Builtin.");
  }
}

void OCLAccHW::visitCmpInst(CmpInst &I) {
  // FIXME
  cmp_p C = makeHWBB<Compare>(I.getParent(), &I, "Compare");
}

void OCLAccHW::visitPHINode(PHINode &I) {
  mux_p HWM = makeHWBB<Mux>(I.getParent(), &I, I.getName());

  for (unsigned i = 0; i < I.getNumIncomingValues(); ++i) {
    const BasicBlock *BB = I.getIncomingBlock(i);
    const Value *V = I.getIncomingValue(i);

    port_p HWP = getHW<Port>(I.getParent(), V);
    block_p HWB = getBlock(BB);

    HWM->addIn(HWP, HWB);
    connect(HWP, HWM);
  }
}

#ifdef TYPENAME
#undef TYPENAME
#endif

#undef DEBUG_TYPE
