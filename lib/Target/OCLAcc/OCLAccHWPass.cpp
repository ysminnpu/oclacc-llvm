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
#include "OCLAccTargetMachine.h"
#include "OCLAccGenSubtargetInfo.inc"
#include "OpenCLDefines.h"

#include "HW/HW.h"
#include "HW/Arith.h"
#include "HW/typedefs.h"
#include "HW/Design.h"
#include "HW/Visitor/Dot.h"
#include "HW/Visitor/Vhdl.h"
#include "HW/Visitor/Latency.h"

#include <cxxabi.h>
#define TYPENAME(x) abi::__cxa_demangle(typeid(x).name(),0,0,NULL)

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
 // AU.addRequired<CreateBlocksPass>();
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
  for (const Function &KernelFunction: M.getFunctionList()) {

    // Omit declarations, e.g. to built-in functions.
    if (KernelFunction.isDeclaration())
      continue;

    handleKernel(KernelFunction);

  }

  if (GenerateDot) {
    DotVisitor V;
    Design.accept(V);
  }

  return false;
}

/// \breif For each Kernel Function create Arguments and BasicBlocks.
///
/// TODO Handle Work Item Kernels correctly using KernelMDPass
void OCLAccHWPass::handleKernel(const Function &F) {
  //
  bool isWorkItemKernel=true;
  std::string KernelName = F.getName();

  kernel_p HWKernel = makeKernel(&F, KernelName, isWorkItemKernel);
  Design.addKernel(HWKernel);

  // Arguments must be represented by ScalarPorts or StreamPorts
  for (const Argument &Arg : F.getArgumentList()) {
    handleArgument(Arg);
  }

  // For each BasicBlock, create Ports where required and generate all HWNodes
  // for the instructions.
  for (const BasicBlock &BB : F.getBasicBlockList()) {
    handleBBPorts(BB);

    // LLVM does not have a const instvisitor interface but we want to preserve
    // constness as much as possible for performance reasons.
    // FIXME Find a better solution
    visit(const_cast<BasicBlock&>(BB));
  }

}

/// \brief Walk through BBs, set inputs and outputs
///
/// Walk through all instructions, visit their uses and check if they were
/// defined in that BB
void OCLAccHWPass::handleBBPorts(const BasicBlock &BB) {
  block_p HWBB = makeBlock(&BB,BB.getName());

  llvm::SmallPtrSet<const Value *, 1024> VS;
  for (const Instruction &I : BB.getInstList()) {
    for (const Use &U : I.operands()) {
      VS.insert(U.get());
    }
  }

  // find values not being defined in the block
  for (const Value *V : VS) {
    if (const Instruction *I = dyn_cast<Instruction>(V)) {
      const BasicBlock *DefBB = I->getParent();
      if (DefBB == &BB)
        continue;

      base_p HWI = getHW<HW>(I);
      block_p HWDef = getBlock(DefBB);

      scalarport_p HWP = makeHW<ScalarPort>(I, I->getName(), 0);

      HWP->appIn(HWI);
      HWI->appOut(HWP);

      // Add Port to defining and using Block
      HWDef->addOutScalar(HWP);
      HWBB->addInScalar(HWP);
    } else if (const Argument *A = dyn_cast<Argument>(V)) {

    } else if (const Constant *C = dyn_cast<Constant>(V)) {
 //     createConstant();
    } else {
        errs() << "Value " << V->getName() << TYPENAME(V) << " no Instruction/Argument\n";
    }
  }
}

/// \brief Create ScalarInput/InputStream for Kernel
///
/// Depending on its type, InScalar (int, float) or Streams referencing
/// global memory references are created and assigned to the kernel.
///
/// FIXME Actual datawidth has to be adapted
void OCLAccHWPass::handleArgument(const Argument &A) {
  Type *AType = A.getType();
  std::string Name = A.getName().str();

  kernel_p HWKernel = getKernel(A.getParent());

  // Arguments are either actual values directly passed to the kernel or
  // references to memory.
  if (AType->isIntegerTy()) {
    scalarport_p Scalar = makeHW<ScalarPort>(&A, Name, AType->getScalarSizeInBits());
    HWKernel->addInScalar(Scalar);
    errs() << Name << " is InScalar of size " << AType->getScalarSizeInBits() << "\n";
  }
  else if (AType->isFloatingPointTy()) {
    Datatype FloatType=Invalid;
    if (AType->isFloatTy()) FloatType = Float;
    if (AType->isFloatTy()) FloatType = Float;
    if (AType->isDoubleTy()) FloatType = Double;

    scalarport_p Scalar = makeHW<ScalarPort>(&A, Name, AType->getScalarSizeInBits());
    HWKernel->addInScalar(Scalar);
    errs() << Name << " is InScalar of size " << AType->getScalarSizeInBits() << "\n";
  }
  else if (AType->isPointerTy()) {
    bool isRead = false;
    bool isWritten = false;

    PointerType *PType = static_cast<PointerType*>(AType);
    unsigned LAS = PType->getAddressSpace();
    ocl::AddressSpace AS = static_cast<ocl::AddressSpace>(LAS);

    switch (AS) {
      case ocl::AS_GLOBAL:
        break;
      case ocl::AS_LOCAL:
        break;
      default:
        llvm_unreachable("Invalid AddressSpace");
    }

    if ( A.onlyReadsMemory() ) {
      isRead = true;
    }
    else {
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

    Type *ElementType   = AType->getPointerElementType();
    unsigned SizeInBits = ElementType->getScalarSizeInBits();

    if (!isWritten && !isRead)
      errs() << "omitting unused Argument " << A.getName() << "\n";
    else {
      if (isWritten) {
        streamport_p S = makeHW<StreamPort>(&A, Name, SizeInBits, AS);
        HWKernel->addOutStream(S);
        DEBUG_WITH_TYPE("OCLAccHWPass", dbgs() << Name << " is OutStream of size " <<  SizeInBits << "\n");
      }
      if (isRead) {
        streamport_p S = makeHW<StreamPort>(&A, Name, SizeInBits, AS);
        HWKernel->addInStream(S);
        DEBUG_WITH_TYPE("OCLAccHWPass", dbgs() << Name << " is InStream of size " <<  SizeInBits << "\n");
      }
    }
  }
  else
    llvm_unreachable("Unknown Argument Type");
}

void OCLAccHWPass::visitBinaryOperator(Instruction &I) {
  Value *IVal = &I;
  Type *IType = I.getType();

  if (ValueMap.find(IVal) != ValueMap.end()) {
    errs() << IVal->getName() << "\n";
    llvm_unreachable("already in ValueMap.");
  }

  std::string IName = I.getName();

  base_p HWOp;

  //TODO change to actual data width
  size_t Bits = IType->getPrimitiveSizeInBits();

  if (IType->isVectorTy())
    TODO("Impelent vector types");

  switch ( I.getOpcode() ) {
    case Instruction::Add:
      HWOp = makeHW<Add>(IVal, IName+"Add",Bits);
      break;
    case Instruction::FAdd:
      HWOp = makeHW<FAdd>(IVal, IName+"FAdd",Bits);
      break;
    case Instruction::Sub:
      HWOp = makeHW<Sub>(IVal,IName+"Sub",Bits);
      break;
    case Instruction::FSub:
      HWOp = makeHW<FSub>(IVal,IName+"FSub",Bits);
      break;
    case Instruction::Mul:
      HWOp = makeHW<Mul>(IVal,IName+"Mul",Bits);
      break;
    case Instruction::FMul:
      HWOp = makeHW<FMul>(IVal,IName+"FMul",Bits);
      break;
    case Instruction::UDiv:
      HWOp = makeHW<UDiv>(IVal,IName+"UDiv",Bits);
      break;
    case Instruction::SDiv:
      HWOp = makeHW<SDiv>(IVal,IName+"SDiv",Bits);
      break;
    case Instruction::FDiv:
      HWOp = makeHW<FDiv>(IVal,IName+"FDiv",Bits);
      break;
    case Instruction::URem:
      HWOp = makeHW<URem>(IVal,IName+"URem",Bits);
      break;
    case Instruction::SRem:
      HWOp = makeHW<SRem>(IVal,IName+"SRem",Bits);
      break;
    case Instruction::FRem:
      HWOp = makeHW<FRem>(IVal,IName+"FRem",Bits);
      break;
      //Logical
    case Instruction::Shl:
      HWOp = makeHW<Shl>(IVal,IName+"Shl",Bits);
      break;
    case Instruction::LShr:
      HWOp = makeHW<LShr>(IVal,IName+"LShr",Bits);
      break;
    case Instruction::AShr:
      HWOp = makeHW<AShr>(IVal,IName+"AShr",Bits);
      break;
    case Instruction::And:
      HWOp = makeHW<And>(IVal,IName+"And",Bits);
      break;
    case Instruction::Or:
      HWOp = makeHW<Or>(IVal,IName+"Or",Bits);
      break;
    case Instruction::Xor:
      HWOp = makeHW<Xor>(IVal,IName+"Xor",Bits);
      break;
    default:
      errs() << "Unknown Binary Operator " << I.getOpcodeName() << "\n";
      llvm_unreachable("Halt.");
      return;
  }

  for (Value *OpVal : I.operand_values() ) {
    if (Constant *ConstValue = dyn_cast<Constant>(OpVal)) {
      //const_p HWConst = createConstant(ConstValue);

      //HWKernel->addConstVal(HWConst);
      //connect(HWConst,HWOp);
    } else {
      base_p HWOperand = getHW<HW>(OpVal);

      connect(HWOperand,HWOp);
    }
  }
}

char OCLAccHWPass::ID = 0;

static RegisterPass<OCLAccHWPass> X("oclacc-hw",
    "Create HW Tree.");

} // end namespace llvm

#ifdef TYPENAME
#undef TYPENAME
#endif


