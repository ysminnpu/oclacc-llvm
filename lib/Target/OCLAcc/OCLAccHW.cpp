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

  // Arguments must be represented by ScalarPorts and StreamPorts
  for (const Argument &Arg : F.getArgumentList()) {
    handleArgument(Arg);
  }

  // For each BasicBlock, create Ports where required and generate all HWNodes
  // for the instructions.
  for (const BasicBlock &BB : F.getBasicBlockList()) {
    visit(const_cast<BasicBlock&>(BB));
  }

  HWKernel->dump();

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
  block_p HWBB = makeBlock(&BB,BB.getName());

  FindAllPaths &AP = getAnalysis<FindAllPaths>(*(BB.getParent()));

  std::vector<const Value *> VS;
  for (const Instruction &I : BB.getInstList()) {
    for (const Use &U : I.operands()) {
      const Value *V = U.get();
      if (isa<Constant>(V)
          || isa<BasicBlock>(V)
          ) continue;

      VS.push_back(V);
    }
  }

  std::sort(VS.begin(), VS.end());
  VS.erase(std::unique(VS.begin(), VS.end()), VS.end());

  // Find values not being defined in BB
  for (const Value *V : VS) {
    if (const Instruction *I = dyn_cast<Instruction>(V)) {
      const BasicBlock *DefBB = I->getParent();
      
      // Local value
      if (DefBB == &BB)
        continue;

      // The current block BB uses a value defined in a different block. We have
      // to:
      // - find the complete path from the definition's block to the current
      // - create a single ScalarPort SP to connect both
      // - add SP as OutScalar in DefBB
      // - add SP as InScalar in BB
      // - connect the defining value with the OutScalar
      // - connect the user with the InScalar
      // - create a blockValueMap entry BB->(V->ScalarPort)

      base_p HWI = getHW<HW>(I);
      block_p HWDef = getBlock(DefBB);

      Type *VT = V->getType();

      // Find all possible paths from DefBB to BB
      for (const FindAllPaths::SinglePathTy P : AP.getPathFromTo(DefBB, &BB)) {

        for (FindAllPaths::SinglePathConstIt FromBBIt = P.begin(), ToBBIt = std::next(FromBBIt); 
            ToBBIt != P.end(); 
            FromBBIt = ToBBIt, ++ToBBIt ) 
        {
          scalarport_p HWP = makeHWBB<ScalarPort>(&BB, I, I->getName(), VT->getScalarSizeInBits(), getDatatype(VT));
          connect(HWI, HWP);

          // Proceed HWI to connect potential next blocks port to
          HWI = HWP;

          // Add Port to defining and using Block
          HWDef->addOutScalar(HWP);
          HWBB->addInScalar(HWP);
        }
      }

    } 
    else if (const Argument *A = dyn_cast<Argument>(V)) {
      // get Kernel
      kernel_p HWK = getKernel(A->getParent());

      //inputs
      for (const scalarport_p HWP : HWK->getInScalars() ) {
        if (HWP->getIR() == A) {
          HWBB->addInScalar(HWP);
          BlockValueMap[&BB][A] = HWP ;
        }
      }
      for (const streamport_p HWP : HWK->getInStreams() ) {
        if (HWP->getIR() == A) {
          HWBB->addInStream(HWP);
          BlockValueMap[&BB][A] = HWP ;
        }
      }

      // outputs
      for (const scalarport_p HWP : HWK->getOutScalars() ) {
        if (HWP->getIR() == A) {
          HWBB->addOutScalar(HWP);
          BlockValueMap[&BB][A] = HWP ;
        }
      }
      for (const streamport_p HWP : HWK->getOutStreams() ) {
        if (HWP->getIR() == A) {
          HWBB->addOutStream(HWP);
          BlockValueMap[&BB][A] = HWP ;
        }
      }
    } 
    else if (const Constant *C = dyn_cast<Constant>(V)) {
        TODO("handle constant");
    } else {
        errs() << "Value " << V->getName() << " :" << TYPENAME(V) << " no Instruction/Argument\n";
        llvm_unreachable("stop.");
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
/// OpenCL 2.0 pp 34: allowed address space for variables is _private,
/// pointers may be global, local or constant
void OCLAccHW::handleArgument(const Argument &A) {
  Type *AType = A.getType();
  const std::string Name = A.getName();

  kernel_p HWKernel = getKernel(A.getParent());

  if (AType->isIntegerTy() || AType->isFloatingPointTy()) {
    scalarport_p S = makeHW<ScalarPort>(&A, Name, AType->getScalarSizeInBits(), getDatatype(AType));
    HWKernel->addInScalar(S);
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

    Type *ElementType   = AType->getPointerElementType();
    unsigned SizeInBits = ElementType->getScalarSizeInBits();

    if (!isWritten && !isRead)
      DEBUG(dbgs() << "omitting unused Argument " << A.getName() << "\n");
    else {
      const Datatype D = getDatatype(ElementType);
      if (isWritten) {
        streamport_p S = makeHW<StreamPort>(&A, Name, SizeInBits, AS, D);
        S->setParent(HWKernel);
        HWKernel->addOutStream(S);
        DEBUG(dbgs() << "Argument '" << Name << "' is OutStream (" << Strings_Datatype[D] << ":" <<  SizeInBits << ")\n");
      }
      if (isRead) {
        streamport_p S = makeHW<StreamPort>(&A, Name, SizeInBits, AS, D);
        HWKernel->addInStream(S);
        S->setParent(HWKernel);
        DEBUG(dbgs() << "Argument '" << Name << "' is InStream (" << Strings_Datatype[D] << ":" <<  SizeInBits << ")\n");
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

  errs() << I.getName() << " in block " << I.getParent()->getName() << "\n";

  std::string IName = I.getName();

  Function *F = I.getParent()->getParent();

  BitWidthAnalysis &BW = getAnalysis<BitWidthAnalysis>(*F);
  std::pair<int, Loopus::ExtKind> W = BW.getBitWidth(&I);

  base_p HWOp;

  unsigned Bits = W.first;

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
        HWOp = makeHW<FAdd>(IVal, IName, M, E);
        break;
      case Instruction::FMul:
        HWOp = makeHW<FMul>(IVal,IName, M, E);
        break;
      case Instruction::FRem:
        HWOp = makeHW<FRem>(IVal,IName, M, E);
        break;
      case Instruction::FSub:
        HWOp = makeHW<FSub>(IVal,IName, M, E);
        break;
      case Instruction::FDiv:
        HWOp = makeHW<FDiv>(IVal,IName, M, E);
        break;
    }
  } else {
    // It depends on the Instruction if values have to be interpreted as signed or
    // unsigned.
    switch ( I.getOpcode() ) {
      case Instruction::Add:
        HWOp = makeHW<Add>(IVal, IName,Bits);
        break;
      case Instruction::Sub:
        HWOp = makeHW<Sub>(IVal,IName,Bits);
        break;
      case Instruction::Mul:
        HWOp = makeHW<Mul>(IVal,IName,Bits);
        break;
      case Instruction::UDiv:
        HWOp = makeHW<UDiv>(IVal,IName,Bits);
        break;
      case Instruction::SDiv:
        HWOp = makeHW<SDiv>(IVal,IName,Bits);
        break;
      case Instruction::URem:
        HWOp = makeHW<URem>(IVal,IName,Bits);
        break;
      case Instruction::SRem:
        HWOp = makeHW<SRem>(IVal,IName,Bits);
        break;
        //Logical
      case Instruction::Shl:
        HWOp = makeHW<Shl>(IVal,IName,Bits);
        break;
      case Instruction::LShr:
        HWOp = makeHW<LShr>(IVal,IName,Bits);
        break;
      case Instruction::AShr:
        HWOp = makeHW<AShr>(IVal,IName,Bits);
        break;
      case Instruction::And:
        HWOp = makeHW<And>(IVal,IName,Bits);
        break;
      case Instruction::Or:
        HWOp = makeHW<Or>(IVal,IName,Bits);
        break;
      case Instruction::Xor:
        HWOp = makeHW<Xor>(IVal,IName,Bits);
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
      base_p HWOperand = getHW<HW>(OpVal);

      connect(HWOperand,HWOp);
    }
  }
}

void OCLAccHW::visitLoadInst(LoadInst  &I)
{
  std::string Name = I.getName().str();
  Value *AddrVal = I.getPointerOperand();

  unsigned AddrSpace = I.getPointerAddressSpace();

  if ( AddrSpace != ocl::AS_GLOBAL && AddrSpace != ocl::AS_LOCAL )
    llvm_unreachable("NOT_IMPLEMENTED: Only global and local address space supported.");

  //Get Address to store at
  streamindex_p HWStreamIndex = getHW<StreamIndex>(AddrVal);
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

  HWStreamIndex->setName(HWStream->getName());
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

  Value *DataVal = I.getValueOperand();
  Value *AddrVal = I.getPointerOperand();

  // Check Address Space
  unsigned LAS = I.getPointerAddressSpace();
  ocl::AddressSpace AS = static_cast<ocl::AddressSpace>(LAS);

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
  
  if ( Constant *ConstValue = dyn_cast<Constant>(DataVal) ) {
    const_p HWConst = makeConstant(ConstValue, &I);
    HWData = HWConst;
  } else {
    HWData = getHW<HW>(DataVal);
  }

  //Get Address to store at
  HWOut = getHW<HW>(AddrVal);

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

  streamport_p HWBase = getHW<StreamPort>(BaseValue);

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
    HWStreamIndex = makeHW<StaticStreamIndex>(&I, Name, HWBase, C, IntConst->getValue().getActiveBits());
  } else {
    HWIndex = getHW<HW>(I.getParent(), IndexValue);

    // No need to add StreamIndex to BlockValueMap so create object directly
    HWStreamIndex = makeHW<DynamicStreamIndex>(&I, Name, HWBase, HWIndex);

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
const_p OCLAccHW::makeConstant(Constant *C, Instruction *I) {
  BasicBlock * BB = I->getParent();
  block_p HWBlock = getBlock(BB);
  Function *F = BB->getParent();

  BitWidthAnalysis &BWA = getAnalysis<BitWidthAnalysis>(*F);
  Loopus::BitWidthRetTy BW = BWA.getBitWidth(C, I);

  std::stringstream CName;
  const_p HWConst;

  Type *CType = C->getType();

  int Bits = BW.first;

  if (ConstantInt *IConst = dyn_cast<ConstantInt>(C)) {
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
  else if (ConstantFP *FConst = dyn_cast<ConstantFP>(C)) {
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
  cmp_p C = makeHW<Compare>(&I, "Compare");
}

void OCLAccHW::visitPHINode(PHINode &I) {
  mux_p HWM = makeHW<Mux>(&I, I.getName());

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
