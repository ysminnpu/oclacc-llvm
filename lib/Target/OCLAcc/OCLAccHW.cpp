#include "llvm/ADT/SmallPtrSet.h"
#include "llvm/ADT/StringExtras.h"
#include "llvm/ADT/SmallSet.h"
#include "llvm/Analysis/AliasAnalysis.h"
#include "llvm/Analysis/CFG.h"
#include "llvm/Analysis/CFGPrinter.h"
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

#include "OCLAccPasses.h"
#include "OCL/NameMangling.h"
#include "Passes/OpenCLMDKernels.h"
#include "Passes/HDLPromoteID.h"
#include "Passes/ArgPromotionTracker.h"
#include "Passes/BitWidthAnalysis.h"
#include "Passes/FindAllPaths.h"
#include "Passes/SplitBarrierBlocks.h"
#include "Passes/HDLLoopUnroll.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <map>
#include <list>
#include <set>
#include <fstream>
#include <sstream>
#include <unistd.h>

#include "Macros.h"
#include "OCLAccHW.h"
#include "OCLAccTargetMachine.h"
#include "OCLAccGenSubtargetInfo.inc"
#include "OCL/OpenCLDefines.h"

#include "HW/HW.h"
#include "HW/Arith.h"
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

static cl::opt<bool> CfgDot("cfg-dot", cl::desc("Write Dot CFG"), cl::init(false));

// The TargetMachine enqueues all Transformations from which we do not need any
// info except from the transformed Module itself.
INITIALIZE_PASS_BEGIN(OCLAccHW, "oclacc-hw", "Generate OCLAccHW",  false, true)
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

  AU.addRequired<ArgPromotionTracker>();
  AU.addRequired<OpenCLMDKernels>();

  // Analysis
  AU.addRequired<BitWidthAnalysis>();
  AU.addRequired<FindAllPaths>();

  // We do not change the Module any more
  AU.setPreservesAll();
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
  F << "%.pdf: %.ps\n";
  F << "\tps2pdf $<\n";
  F << "%.ps: %.dot\n";
  F << "\tdot -Tps2 $< > $@\n";
  F.close();
}


/// \brief Main HW generation Pass
///
bool OCLAccHW::runOnModule(Module &M) {

  const std::string ModuleName = M.getName();

  DL = M.getDataLayout();

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

  // Allocate global values (OpenCL Local)
  // These may be simple variables or (multi-dimensional) arrays. We handle all
  // similarly to allow uniform handling e.g. of atomic access for all work
  // items.
  for (const GlobalVariable &G : M.getGlobalList()) {
    handleGlobalVariable(G);
  }

  // Process all kernel functions
  OpenCLMDKernels &CLK = getAnalysis<OpenCLMDKernels>();
  std::vector<Function*> Kernels;
  CLK.getKernelFunctions(Kernels);

  for (Function *KF: Kernels) {
    // We do currently not support loops.
    SmallVector<std::pair<const BasicBlock*,const BasicBlock*>, 32 > Result;
    FindFunctionBackedges(*KF, Result);
    assert (Result.empty() && "Handle Loops");

    // Print bitwidth of functions

    FindAllPaths &AP = getAnalysis<FindAllPaths>(*KF);
    AP.dump();

    // Print cfg
    if (CfgDot) {
      FunctionPass *CFG = createCFGPrinterPass();
      CFG->runOnFunction(*KF);
    }
    {
      std::error_code EC;
      std::string FileName = ModuleName+"."+std::string(KF->getName())+".bitwidth";
      raw_fd_ostream File(FileName, EC, llvm::sys::fs::F_RW | llvm::sys::fs::F_Text);
      if (EC) {
        errs() << "Failed to create " << FileName << "(" << __LINE__ << "): " << EC.message() << "\n";
        return -1;
      }
      BitWidthAnalysis &BW = getAnalysis<BitWidthAnalysis>(*KF);
      BW.print(File, KF);
    }


    handleKernel(*KF);

  }

  return false;
}

void OCLAccHW::handleGlobalVariable(const GlobalVariable &G) {
  const std::string Name = G.getName();
  const PointerType *T = G.getType();

  ocl::AddressSpace AS = static_cast<ocl::AddressSpace>(T->getAddressSpace());
  assert(AS == ocl::AS_LOCAL);

  Type *ObjTy = T->getElementType();

  uint64_t Length = 1;

  while (true) {
    SequentialType *ST = dyn_cast<SequentialType>(ObjTy);
    if (!ST) break;

    // ST may be a Pointer, Vector or Array
    assert(!isa<PointerType>(ST) && "TODO Global Pointer Arrays");
    assert(!isa<VectorType>(ST) && "TODO Global Vector Arrays");

    Length *= ST->getArrayNumElements();

    ObjTy = ST->getElementType();
  }

  // Now ObjTy contains the elementTy, which may be still a structure. No
  // special handling needed, we just see structs as large scalars here.
  uint64_t ScalarBitWidth = DL->getTypeAllocSizeInBits(ObjTy);

  ODEBUG("Global " << Name << ": " << Length << "x" << ScalarBitWidth/8 << " Bytes");

  Datatype D = getDatatype(ObjTy);

  streamport_p S = makeHW<StreamPort>(&G, Name, ScalarBitWidth, AS, D, Length);

  //Add the Stream to the Kernel and all BasicBlocks using it.
  std::set<const BasicBlock *> Blocks;
  for (const Use &U : G.uses()) {
    if (const  Instruction *I = dyn_cast<Instruction>(U.getUser())) {
      const BasicBlock *IBB = I->getParent();

      Blocks.insert(IBB);
    }
  }
  for (const BasicBlock *BB : Blocks) {
    BlockValueMap[BB][&G] = S;
  }
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
  const BasicBlock *EntryBB = &(F.getEntryBlock());
  for (const BasicBlock &BB : F.getBasicBlockList()) {
    block_p HWBB = makeBlock(&BB,BB.getName(), &BB == EntryBB);
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
/// defined in that BB. If not, check if it is used by a PHINode to avoid
/// unnecessary ports.
///
void OCLAccHW::visitBasicBlock(BasicBlock &BB) {
  FindAllPaths &AP = getAnalysis<FindAllPaths>(*(BB.getParent()));
  ArgPromotionTracker &AT = getAnalysis<ArgPromotionTracker>();

  block_p HWBB = getBlock(&BB);

  // List of values defined in other BBs and Arguments.
  std::vector<const Value *> VS;
  std::vector<const Argument *> VA;

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
      if (const Instruction *II = dyn_cast<Instruction>(V)) {

        // Only handle Instructions not defined in BB
        if (II->getParent() == &BB)
          continue;


        VS.push_back(II);
      } else if (const Argument *A = dyn_cast<Argument>(V)) {
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
            assert(0);
          }

          VS.push_back(A);
        } else {
          // Arguments which do not have to be visited recursively but only have
          // to be added as Port to BB.
          VA.push_back(A);
        }
      }
      else {
        V->dump();
        assert(0 && "unknown value");
      }
    }
  }

  // Multiple uses of a single Value should result in a single Port. So we
  // eliminate duplicates.
  //
  // std::unique returns an iterator to the new end to delete all remaining,
  // non-unique items
  std::sort(VS.begin(), VS.end());
  VS.erase(std::unique(VS.begin(), VS.end()), VS.end());

  std::sort(VA.begin(), VA.end());
  VA.erase(std::unique(VA.begin(), VA.end()), VA.end());

  for (const Value *I : VS) {
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

    const BasicBlock *DefBB;
    if (const Instruction *II = dyn_cast<Instruction>(I))
      DefBB = II->getParent();
    else if (const Argument *A = dyn_cast<Argument>(I))
      DefBB = &(A->getParent()->getEntryBlock());
    else
      assert(0 && "Value not Instruction or Argument");

    block_p HWDefBB = getBlock(DefBB);
    base_p HWI = getHW<HW>(DefBB, I);

    // The type will be the same for all newly created ports
    const Type *IT = I->getType();
    const std::string Name = I->getName();

    //const FindAllPaths::PathTy &Paths = AP.getPathFromTo(DefBB, &BB);

    // If the current Instruction is a PHINode, we only have to find all paths
    // between the Value's definition and the incoming block of the PHINode.
    const FindAllPaths::PathTy &Paths = AP.getPathForValue(DefBB, &BB, I);

    if (IT->isIntegerTy() || IT->isFloatingPointTy()) {
      for (const FindAllPaths::SinglePathTy P : Paths) {

        for (FindAllPaths::SinglePathConstIt FromBBIt = P.begin(), ToBBIt = std::next(FromBBIt), E = P.end();
            ToBBIt != E;
            FromBBIt = ToBBIt, ++ToBBIt )
        {
          block_p HWFrom = getBlock(*FromBBIt);
          block_p HWTo = getBlock(*ToBBIt);

          scalarport_p HWOut;
          scalarport_p HWIn;

          // The Ports are created with isPipelined==true to indicate that it
          // will be passed from BB to BB and that we have to create additional
          // signals for synchronization.
          //
          if (!HWFrom->containsOutScalarForValue(I)) {
            HWOut = makeHWBB<ScalarPort>(*FromBBIt, I, I->getName(), IT->getScalarSizeInBits(), getDatatype(IT), true);
            HWFrom->addOutScalar(HWOut);
            connect(HWI, HWOut);
          } else
            HWOut = HWFrom->getOutScalarForValue(I);

          if (!HWTo->containsInScalarForValue(I)) {
            HWIn = makeHWBB<ScalarPort>(*ToBBIt, I, I->getName(), IT->getScalarSizeInBits(), getDatatype(IT), true);
            HWTo->addInScalar(HWIn);
          } else
            HWIn = HWTo->getInScalarForValue(I);

          assert(HWOut && HWIn);

          // TODO: use BitWidthAnalysis

          connect(HWOut, HWIn);

          // Proceed HWI to connect potential next blocks port to
          HWI = HWIn;
        }
      }
    }
    else if (IT->isPointerTy()) {
      IT->dump();
      assert(0 && "FIXME: Pointer operand in code.");
    }
  }

  // Handle the remaining Arguments which only have to be added to the block
  // using them. The ports for the Arguments must already exist in the kernel's
  // lists.
  for (const Argument *A : VA) {
    const Type *IT = A->getType();
    const std::string Name = A->getName();

    if (IT->isIntegerTy() || IT->isFloatingPointTy()) {
      if (HWBB->containsInScalarForValue(A))
        continue;

      scalarport_p HWDef = getHW<ScalarPort>(&BB, A);

      HWBB->addInScalar(HWDef);
    }
    else if (IT->isPointerTy()) {
      // nothing to do here, will be done outside of loop.
      continue;
    }
  }
}

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
///  - Eliminated WI functions: propagate them from block to block.
///  - Real scalars: same for all WIs, no propagation between BBs
///  - Streams: like real scalars
///
/// The separation between WI and static scalars is respected when they are used. Their
/// generation here only depends on their type.
///
/// To simplify the connection of the definition and the use of StreamPorts, we
/// also add them to separate lists right now.
///
/// OpenCL 2.0 pp 34: allowed address space for variables is _private,
/// pointers may be global, local or constant
void OCLAccHW::handleArgument(const Argument &A) {
  ArgPromotionTracker &AT = getAnalysis<ArgPromotionTracker>();

  const Type *AType = A.getType();
  const std::string Name = A.getName();

  const Function *F = A.getParent();

  kernel_p HWKernel = getKernel(F);

  const BasicBlock *EB = &(F->getEntryBlock());
  block_p HWEB = getBlock(EB);

  BitWidthAnalysis &BW = getAnalysis<BitWidthAnalysis>(const_cast<Function &>(*F));
  std::pair<int, Loopus::ExtKind> W = BW.getBitWidth(&A);
  unsigned Bits = W.first;

  // Normal Scalars are not pipelined as they have the same value for all
  // WorkItems
  if (AType->isIntegerTy() || AType->isFloatingPointTy()) {
    // Promoted Arguments must be marked for the Kernel and all BBs
    bool isPromoted = AT.isPromotedArgument(&A);
    scalarport_p HWS = makeHW<ScalarPort>(&A, Name, AType->getScalarSizeInBits(), getDatatype(AType), isPromoted);

    HWKernel->addInScalar(HWS);
    ArgMap[&A] = HWS;
    HWS->setParent(HWKernel);

    // We can safely add the WI ScalarPort to the EntryBlock as it will be
    // used later. If we skip that, we have no connection between the
    // definition in the Kernel and its uses in the blocks.
    //
    if (isPromoted) {
      const BasicBlock *EB =  &(A.getParent()->getEntryBlock());
      block_p HWEB = getBlock(EB);
      scalarport_p HWSP = makeHWBB<ScalarPort>(EB, &A, Name, AType->getScalarSizeInBits(), getDatatype(AType), true);
      HWEB->addInScalar(HWSP);

      connect(HWS, HWSP);
    }
    // All other Arguments are connected when used.
  }
  // Create Ports for all Pointers. The direction of the Port will be defined
  // depending on its usage in the BBs.
  else if (const PointerType *PType = dyn_cast<PointerType>(AType)) {
    unsigned AS = PType->getAddressSpace();

    switch (AS) {
      case ocl::AS_GLOBAL:
        break;
      case ocl::AS_LOCAL:
        break;
      case ocl::AS_CONSTANT:
        break;
      default:
        assert(0 && "Invalid AddressSpace");
    }

    const Type *ElementType   = AType->getPointerElementType();

    const Datatype D = getDatatype(ElementType);
    streamport_p S = makeHW<StreamPort>(&A, Name, Bits, static_cast<ocl::AddressSpace>(AS), D);
    ArgMap[&A] = S;
    S->setParent(HWKernel);

    HWKernel->addStream(S);
  }
}

//////////////////////////////////////////////////////////////////////////////
// Visit Functions
//////////////////////////////////////////////////////////////////////////////

void OCLAccHW::visitBinaryOperator(BinaryOperator &I) {
  Value *IVal = &I;
  Type *IType = I.getType();
  const BasicBlock *BB = I.getParent();

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
      assert(0 && "Invalid FP Type");
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
      default:
        assert(0 && "Invalid FP Binary Op");
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
        assert(0 && "Unknown Binary Operator");
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

void OCLAccHW::visitLoadInst(LoadInst &I)
{
  const std::string Name = I.getName().str();
  const Value *AddrVal = I.getPointerOperand();
  const BasicBlock *Parent = I.getParent();

  const block_p HWParent = getBlock(Parent);

  unsigned AddrSpace = I.getPointerAddressSpace();

  if ( AddrSpace != ocl::AS_GLOBAL && AddrSpace != ocl::AS_LOCAL )
    assert(0 && "FIXME: Only global and local address space supported.");

  //Get Address to load from
  streamindex_p HWStreamIndex;
  streamport_p HWStream;

  const GetElementPtrInst *P = dyn_cast<GetElementPtrInst>(AddrVal);
  if (P) {
    HWStreamIndex = getHW<StreamIndex>(Parent, P);
    HWStream = HWStreamIndex->getStream();
  } else {
    HWStream = getHW<StreamPort>(Parent, AddrVal);
    HWStreamIndex = std::make_shared<StaticStreamIndex>("0", HWStream, 0, 1);
    HWStreamIndex->addOut(HWStream);
  }

  assert(HWStreamIndex && "No Index.");
  assert(HWStream && "No Stream");

  // We may have loads with different types using the same StreamIndex.
  const Type *T = I.getType();
  unsigned BitWidth = T->getPrimitiveSizeInBits();

  // TODO: Maybe we should support Non-primitive types
  assert(BitWidth && "FIXME: Type not primitive");

  loadaccess_p HWLoad = makeHWBB<LoadAccess>(Parent, &I, Name, BitWidth, HWStreamIndex);

  connect(HWStreamIndex, HWLoad);

  // Check if the load has the same BitWidth as the port.
  unsigned StreamBitWidth = HWStream->getBitWidth();
  assert(StreamBitWidth >= BitWidth);

  // Streams are global. Tell the Block that it has a load. The load
  // itself keeps a reference to the BasicBlock and the Stream.
  HWStream->addAccess(HWLoad);

  HWParent->addStreamAccess(HWLoad);
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
  // TODO atomic, volatile
  assert(I.isSimple());

  // Stores do not have any name
  const std::string Name = "store";

  const Value *DataVal = I.getValueOperand();
  const Value *AddrVal = I.getPointerOperand();
  const BasicBlock *Parent = I.getParent();

  const block_p HWParent = getBlock(Parent);

  // Check Address Space
  unsigned AS = I.getPointerAddressSpace();

  switch (AS) {
    case ocl::AS_GLOBAL:
      break;
    case ocl::AS_LOCAL:
      break;
    default:
      assert(0 && "Invalid AddressSpace");
  }

  base_p HWData;
  base_p HWOut;

  //Get Data to store

  if (const Constant *ConstValue = dyn_cast<Constant>(DataVal) ) {
    const_p HWConst = makeConstant(ConstValue, &I);
    HWData = HWConst;
  } else {
    HWData = getHW<HW>(Parent, DataVal);
  }

  //Get Address to store at
  HWOut = getHW<HW>(Parent, AddrVal);

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
    assert(0 && "Index base address only streams.");
  }


  // We may have stores with different types using the same StreamIndex.
  const Type *T = I.getValueOperand()->getType();
  unsigned BitWidth = T->getPrimitiveSizeInBits();

  // TODO: Maybe we should support Non-primitive types
  assert(BitWidth);

  //TODO This is the case for local arrays!
  if (! HWStream)
    assert(0 && "Load base address is not a stream");

  // Check if the data to store has the same BitWidth as the port.
  unsigned StreamBitWidth = HWStream->getBitWidth();
  assert(StreamBitWidth >= BitWidth);

  storeaccess_p HWStore = makeHWBB<StoreAccess>(Parent, &I, Name, BitWidth, HWStreamIndex, HWData);

  connect(HWStreamIndex, HWStore);

  HWStream->addAccess(HWStore);

  HWParent->addStreamAccess(HWStore);

  connect(HWData, HWStore);
}

base_p OCLAccHW::computeSequentialIndex(BasicBlock *Parent, Value *IV, Type *NextTy) {
  block_p HWParent = getBlock(Parent);

  const std::string Name = IV->getName();

  base_p HWOffset;

  // If the current index is zero, just skip it
  if (ConstantInt *C = dyn_cast<ConstantInt>(IV)) {
    if (C->isZero()) {
      // do nothing
      ODEBUG("    Size: " << DL->getTypeAllocSize(NextTy));
      ODEBUG("    Index: 0");
    } else {
      // get constant value and multiply by size of current element
      APInt A = APInt(64, C->getZExtValue(), false);
      uint64_t CurrSize = DL->getTypeAllocSize(NextTy);

      ODEBUG("  Size: " << CurrSize);
      ODEBUG("  Index: " << A.toString(10, false));

      A *= APInt(64, CurrSize, false);

      HWOffset = std::make_shared<ConstVal>(
          A.toString(10, false),
          Datatype::Integer,
          A.toString(2, false),
          A.getActiveBits());
    }
  } else {
    // For the index, there must be a HW instance available.
    base_p HWLocalIndex = getHW(Parent, IV);

    // Get the size of the Type
    uint64_t CurrSize = DL->getTypeAllocSize(NextTy);

    ODEBUG("    Size: " << CurrSize);
    ODEBUG("    Index: " << IV->getName());

    // Sizes may be not a power of two, e.g.
    // struct foo {
    //  int a;
    //  int b;
    //  int c;
    // }
    //
    // struct foo bar[123];

    // Insert shifter if size is a power of two or a multiplication otherwise.
    APInt CurrSizeAP(64, CurrSize, false);
    uint64_t AddrWidth = CurrSizeAP.getActiveBits();

    const_p HWSize = std::make_shared<ConstVal>(std::to_string(CurrSize), CurrSizeAP.toString(2, false), AddrWidth);

    int32_t Log = CurrSizeAP.exactLogBase2();
    if (Log != -1) {
      // Power of two, use left shift
      AddrWidth = Log + HWLocalIndex->getBitWidth();
      std::string IndexName = Name+"_shift";

      shl_p HWShift = std::make_shared<Shl>(IndexName, AddrWidth);
      HWParent->addOp(HWShift);

      HWParent->addConstVal(HWSize);

      connect(HWLocalIndex, HWShift);
      connect(HWSize,HWShift);

      HWOffset = HWShift;
    } else {
      // No power of two, use multiplication
      AddrWidth = CurrSizeAP.getActiveBits() + HWLocalIndex->getBitWidth();
      std::string IndexName = Name+"_mul";

      mul_p HWMul = std::make_shared<Mul>(IndexName, AddrWidth);
      HWParent->addOp(HWMul);

      HWParent->addConstVal(HWSize);

      connect(HWLocalIndex, HWMul);
      connect(HWSize, HWMul);

      HWOffset = HWMul;
    }
  }
  return HWOffset;
}

base_p OCLAccHW::computeStructIndex(BasicBlock *Parent, Value *IV, Type *NextTy) {
  base_p HWOffset;

  return HWOffset;
}


/// \brief Create StaticStreamIndex or DynamicStreamIndex used by Load or
/// StoreInst.
///
/// By now, we do not know how this stream port is used (load and/or store
/// index). The index used is a separate member of these classes while the
/// load/store connects the index with re stream, either with a value used as
/// input (st) or an output (ld).
///
void OCLAccHW::visitGetElementPtrInst(GetElementPtrInst &I) {

  BasicBlock *Parent = I.getParent();
  Value *InstValue = &I;
  Value *BaseValue = I.getPointerOperand();

  SequentialType *IType = I.getType();

  if (! I.isInBounds() )
    assert(0 && "Not in Bounds.");

  const std::string Name = I.getName();

  // The pointer base can either be a local Array or a input stream
  unsigned BaseAddressSpace = I.getPointerAddressSpace();
  assert((BaseAddressSpace == ocl::AS_GLOBAL || BaseAddressSpace == ocl::AS_LOCAL)
      && "Only global and local address space supported." );

  streamport_p HWBase = getHW<StreamPort>(I.getParent(), BaseValue);

  ODEBUG("GEP " << Name);

  // Walk throught the indices. The GEP instruction may have multiple indices
  // when arrays or structures are accessed.

  // Constant zero address
  if (I.hasAllZeroIndices()) {
    streamindex_p HWStreamIndex = makeHWBB<StaticStreamIndex>(I.getParent(), InstValue, Name, HWBase, 0, 1);

    BlockValueMap[Parent][InstValue] = HWStreamIndex;

    ODEBUG("  Zero Index " << HWStreamIndex->getUniqueName());

    return;
  }

  // We currently only support GEP instructions with the same Type as their
  // Pointer, no Vectors.
  Type *BaseType = BaseValue->getType();
  PointerType *BasePointer = dyn_cast<PointerType>(BaseType);

  assert(BasePointer->isValidElementType(I.getType()) && "Pointer type differs from GEP Type");

  // When we walk through the indices, dereference the Types
  Type *NextTy = BasePointer;

  // We can compute the Index at compile time. Walk through all indices and
  // types and compute the Index in an APInt. Finally save its numerical value
  // as StaticStreamIndex.

  // TODO does not work for Structs
  if (I.hasAllConstantIndices()) {
    APInt API;

    for (User::op_iterator II = I.idx_begin(), E = I.idx_end(); II != E; ++II) {

      //TODO
      // The curr Type indexed by II. Update on each iteration;
      assert(NextTy);

      // All are Constants
      ConstantInt *C = cast<ConstantInt>(*II);
      const APInt &A = C->getValue();

      // In bytes, incl padding
      uint64_t CurrSize = DL->getTypeAllocSize(NextTy);

      API += A * APInt(64, CurrSize, false);

      // Correctnes of cast will be checked within the next Index operand
      SequentialType *ST = dyn_cast<SequentialType>(NextTy);
      if (ST)
        NextTy = ST->getElementType();
      else NextTy = nullptr;
    }

    uint64_t Index = API.getSExtValue();

    streamindex_p HWStreamIndex = makeHWBB<StaticStreamIndex>(I.getParent(), InstValue, Name, HWBase, 0, 1);

    BlockValueMap[Parent][InstValue] = HWStreamIndex;

    ODEBUG("  static index " << HWStreamIndex->getUniqueName() << " = " << Index);

    return;
  }

  // Mixed indices, so we have to arithmetically compute the address at runtime.
  // Use shifter for the multiplication with the scalar sizes. Indizes can refer
  // to SequentialTypes or StructTypes. The latter must have constant indices.
  
  Value *CurrValue;
  block_p HWParent = getBlock(Parent);

  base_p HWIndex = nullptr;

  int IndexNo = 0;
  for (User::op_iterator II = I.idx_begin(), E = I.idx_end(); II != E; ++II, IndexNo++) {
    Value *IV = *II;
    base_p HWOffset = nullptr;

    // Correctnes of cast will be checked within the next Index operand
    if (SequentialType *ST = dyn_cast<SequentialType>(NextTy)) {
      NextTy = ST->getElementType();
      HWOffset = computeSequentialIndex(Parent, IV, NextTy);
    }
    else if (StructType *ST = dyn_cast<StructType>(NextTy)) {
      NextTy = ST->getElementType(0);
      HWOffset = computeStructIndex(Parent, IV, NextTy);
    } else
      assert(0 && "Invalid Type");

    DEBUG(
        dbgs() << "[" << DEBUG_TYPE << "] " << "  Index " << IndexNo << ": ";
        NextTy->print(dbgs());
        dbgs() << "\n";
        );

    // We have no offset if the fist indices are zero
    if (HWOffset) {
      // If we already have an address computation, add the current index
      if (HWIndex) {
        // The Address may grow by one bit
        uint64_t AddrWidth = std::max(HWOffset->getBitWidth(), HWIndex->getBitWidth())+1;
        std::string IndexName = Name+"_"+std::to_string(IndexNo)+"_add";

        base_p HWAdd = std::make_shared<Add>(IndexName, AddrWidth);
        HWParent->addOp(HWAdd);

        connect(HWOffset, HWAdd);

        HWIndex = HWAdd;
      } else
        HWIndex = HWOffset;
    }
  }

  streamindex_p HWStreamIndex = makeHWBB<DynamicStreamIndex>(Parent, InstValue, Name, HWBase, HWIndex, HWIndex->getBitWidth());

  connect(HWIndex, HWStreamIndex);

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

  std::string CName;
  const_p HWConst;

  const Type *CType = C->getType();

  // BitWidth may be -1 and BW.second set to Loopus::ExtKind::Undef
  int BitWidth = BW.first;

  if (const ConstantInt *IConst = dyn_cast<ConstantInt>(C)) {

    const APInt Int = IConst->getValue();

    switch (BW.second) {
      case Loopus::SExt:
        {
          const std::string S = Int.toString(2, true);
          CName = std::to_string(Int.getSExtValue());
          HWConst = std::make_shared<ConstVal>(CName, S, BitWidth);
          break;
        }
      case Loopus::ZExt:
        {
          const std::string U = Int.toString(2, false);
          CName = std::to_string(Int.getZExtValue());
          HWConst = std::make_shared<ConstVal>(CName, U, BitWidth);
          break;
        }
      case Loopus::OneExt:
        {
          TODO("makeConstant OneExt");
          const std::string S = Int.toString(2, true);
          CName = std::to_string(Int.getSExtValue());
          HWConst = std::make_shared<ConstVal>(CName, S, BitWidth);
          break;
        }
      default:
        // Fall back to sign extend.
        // FIXME:
        // The following code did not generate valid Bits:
        // %tmp12 = icmp sgt i32 %get_global_id_1, 0
        {
          const std::string S = Int.toString(2, true);
          CName = std::to_string(Int.getSExtValue());
          HWConst = std::make_shared<ConstVal>(CName, S, Int.getMinSignedBits());
          break;
        }
    }
  }
  else if (const ConstantFP *FConst = dyn_cast<ConstantFP>(C)) {
    assert(BW.second != Loopus::FPNoExt && "Constant is FP Type but FPNoExt is not set");

    const APFloat &Float = FConst->getValueAPF();

    if (CType->isHalfTy())
      assert(0 &&"Half floating point type not supported");
    else if (CType->isFloatTy()) {
      const APInt Bits = Float.bitcastToAPInt();

      CName = std::to_string(Float.convertToFloat());
      const std::string V = Bits.toString(2, false);

      HWConst = std::make_shared<ConstVal>(CName, V, Bits.getBitWidth());

    } else if (CType->isDoubleTy()) {
      const APInt Bits = Float.bitcastToAPInt();

      CName = std::to_string(Float.convertToDouble());
      const std::string V = Bits.toString(2, false);

      HWConst = std::make_shared<ConstVal>(CName, V, Bits.getBitWidth());
    } else
      assert(0 && "Unknown floating point type");
  } else
  {
    assert(0 &&"Unsupported Constant Type");
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

  assert(ocl::NameMangling::isKnownName(CN) && "Unknown or invalid Builtin");

  if (ocl::NameMangling::isWorkItemFunction(CN)) {
    assert(0 && "Run pass to inline WorkItem builtins");
  }

  // If barriers are used, the attributes reqd or max workgroup size must be set
  // to avoid unnecessary hardware generation.
  if (ocl::NameMangling::isSynchronizationFunction(CN)) {
    BasicBlock* BB = I.getParent();
    Function *F = BB->getParent();

    OpenCLMDKernels &CLK = getAnalysis<OpenCLMDKernels>();
    unsigned Dim0 = CLK.getRequiredWorkGroupSize(*F, 0);
    unsigned Dim1 = CLK.getRequiredWorkGroupSize(*F, 1);
    unsigned Dim2 = CLK.getRequiredWorkGroupSize(*F, 2);

    ODEBUG("Required WorkGroupSize: (" << Dim0 << "," << Dim1 << "," << Dim2 << ")");
  }
}

void OCLAccHW::visitCmpInst(CmpInst &I) {
  // We currently directly use the llvm Predicate.
  Compare::PredTy P = static_cast<Compare::PredTy>(I.getPredicate());

  cmp_p HWC;
  if (I.isFPPredicate()) {
    HWC = makeHWBB<FPCompare>(I.getParent(), &I, I.getName() , P);
  } else {
    HWC = makeHWBB<IntCompare>(I.getParent(), &I, I.getName() , P);
  }

  for (const Use &U : I.operands()) {
    const Value *V = U.get();

    base_p HWOp;
    if (const Constant *ConstValue = dyn_cast<Constant>(V) ) {
      HWOp = makeConstant(ConstValue, &I);
    } else {
      HWOp = getHW<HW>(I.getParent(), V);
    }

    connect(HWOp, HWC);
  }

}

/// \brief Create Multiplexer for all PHI Inputs
///
void OCLAccHW::visitPHINode(PHINode &I) {
  BitWidthAnalysis &BW = getAnalysis<BitWidthAnalysis>(const_cast<Function &>(*(I.getParent()->getParent())));
  std::pair<int, Loopus::ExtKind> W = BW.getBitWidth(&I);

  const BasicBlock *BB = I.getParent();
  const block_p HWBB = getBlock(BB);

  mux_p HWM = makeHWBB<Mux>(BB, &I, I.getName(), W.first);

  ODEBUG("Block " << BB->getName());
  ODEBUG("\tPHI " << I.getName());
  for (unsigned i = 0; i < I.getNumIncomingValues(); ++i) {
    const BasicBlock *FromBB = I.getIncomingBlock(i);
    const Value *V = I.getIncomingValue(i);

    port_p HWP = getHW<Port>(BB, V);
    block_p HWB = getBlock(FromBB);

    // All incoming blocks should already have been visited, so we can use
    // Conds and NegConds of BB to look for the IncomingBlock and the condition.

    const base_p Cond = HWBB->getCondReachedByBlock(HWB);
    const base_p NegCond = HWBB->getNegCondReachedByBlock(HWB);

    if (!Cond && !NegCond) {
      // Unconditional branch from FromBB to BB
      const_p HWConst = std::make_shared<ConstVal>("1", "1", 1);
      HWBB->addConstVal(HWConst);
      HWM->addIn(HWP, HWConst);
      connect(HWConst, HWM);
    } else {
      assert(Cond && !NegCond || !Cond && NegCond);
      DEBUG(
      ODEBUG("\t\tFrom Block " << FromBB->getName());
      if (Cond)
        ODEBUG("\t\t\t" << Cond->getUniqueName());
      if (NegCond)
        ODEBUG("\t\t\t!" << NegCond->getUniqueName());

      );

      if (Cond) {
        HWM->addIn(HWP, Cond);
      }

      if (NegCond) {
        HWM->addIn(HWP, NegCond);
      }

      connect(HWP, HWM);
    }
  }
}

//
//bb0:
// %12 =
// %13 =
// %cond =
// br %cond, label %bb1, %bb2
//
//bb1:
// br label %bb2
//
//bb2:
// %13 = phi [%12, bb1], [%13, bb2] ...
//
///
/// \brief Set conditions for each block
void OCLAccHW::visitBranchInst(BranchInst &I) {
  const BasicBlock *BB = I.getParent();
  block_p HWBB = getBlock(BB);

  if (I.isUnconditional()) {
    // No condition, so all Ports can be used when ready.
    const_p HWConst = std::make_shared<ConstVal>("true", "1", 1);
    HWBB->addConstVal(HWConst);

    BasicBlock *SuccBB = I.getSuccessor(0);

    block_p HWSucc = getBlock(SuccBB);

    scalarport_p HWOut = makeHWBB<ScalarPort>(BB, &I, "uncond", 1, oclacc::Integer, true);
    connect(HWConst, HWOut);
    HWBB->addOutScalar(HWOut);

    scalarport_p HWIn = makeHWBB<ScalarPort>(SuccBB, &I, "uncond", 1, oclacc::Integer, true);
    HWSucc->addInScalar(HWIn);

    connect(HWOut, HWIn);

    HWSucc->addCond(HWIn, HWBB);
    return;
  }


  const Value *Cond = I.getCondition();
  base_p HWCond = getHW<HW>(BB, Cond);

  const BasicBlock *TrueBB = I.getSuccessor(0);
  const BasicBlock *FalseBB = I.getSuccessor(1);

  block_p HWTrue = getBlock(TrueBB);
  block_p HWFalse = getBlock(FalseBB);

  const Type *CIT = Cond->getType();

  bool isCondInTrue = isValueInBB(TrueBB, Cond);
  bool isCondInFalse = isValueInBB(FalseBB, Cond);

  scalarport_p HWPort, HWCondTrue, HWCondFalse;

  // If the condition is already used by other ports, omit the creation of a new
  // OutScalar. Otherwise, create a new Output.
  if (!HWBB->containsOutScalarForValue(Cond)) {
    HWPort = makeHWBB<ScalarPort>(BB, Cond, Cond->getName(), CIT->getScalarSizeInBits(), getDatatype(CIT), true);
    HWBB->addOutScalar(HWPort);
    connect(HWCond, HWPort);
  } else {
    HWBB->getOutScalarForValue(Cond);
  }

  // Create inputs like normal Inputs but add them as Condition to the Block if
  // Cond is not already valid in the successor Blocks
  if (isValueInBB(TrueBB, Cond)) {
    HWCondTrue = getHW<ScalarPort>(TrueBB, Cond);
  } else {
    HWCondTrue = makeHWBB<ScalarPort>(TrueBB, Cond, Cond->getName(), CIT->getScalarSizeInBits(), getDatatype(CIT), true);
    HWTrue->addInScalar(HWCondTrue);
    connect(HWPort, HWCondTrue);
  }

  if (isValueInBB(FalseBB, Cond)) {
    HWCondFalse = getHW<ScalarPort>(FalseBB, Cond);
  } else {
    HWCondFalse = makeHWBB<ScalarPort>(FalseBB, Cond, Cond->getName(), CIT->getScalarSizeInBits(), getDatatype(CIT), true);
    HWFalse->addInScalar(HWCondFalse);
    connect(HWPort, HWCondFalse);
  }


  // The condition must be added as OutScalar to BB and InScalar to HWTrue and
  // HWFalse if the Value is not already valid in these Blocks.

  // Set the conditions for each successor
  HWTrue->addCond(HWCondTrue, HWBB);
  HWFalse->addNegCond(HWCondFalse, HWBB);

}

#ifdef TYPENAME
#undef TYPENAME
#endif

#undef DEBUG_TYPE
