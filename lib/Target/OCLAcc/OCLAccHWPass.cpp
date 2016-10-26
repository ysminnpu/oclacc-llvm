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

#include "llvm/Transforms/Loopus.h"
#include "../../../lib/Transforms/Loopus/OpenCLMDKernels.h"

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
#include "OCLAccHWPass.h"
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

#include <cxxabi.h>
#define TYPENAME(x) abi::__cxa_demangle(typeid(x).name(),0,0,NULL)

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
  AU.addRequired<OpenCLMDKernels>();
  //AU.addRequired<CreateBlocksPass>();
  AU.setPreservesAll();
}

void OCLAccHWPass::createMakefile() {
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
bool OCLAccHWPass::runOnModule(Module &M) {

  StringRef ModuleName = M.getName();

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

  // Process all kernel functions
  for (const Function &KernelFunction: M.getFunctionList()) {

    // Omit declarations, e.g. to built-in functions.
    if (KernelFunction.isDeclaration())
      continue;

    handleKernel(KernelFunction);
  }

  return false;
}

/// \breif For each Kernel Function create Arguments and BasicBlocks.
///
/// TODO Handle Work Item Kernels correctly using KernelMDPass
void OCLAccHWPass::handleKernel(const Function &F) {
  bool isWorkItemKernel=true;
  std::string KernelName = F.getName();

  // Set the new Kernel as global Kernel for all visit functions
  kernel_p HWKernel = makeKernel(&F, KernelName, isWorkItemKernel);
  HWDesign.addKernel(HWKernel);

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
void OCLAccHWPass::handleArgument(const Argument &A) {
  Type *AType = A.getType();
  std::string Name = A.getName().str();

  kernel_p HWKernel = getKernel(A.getParent());

  // Arguments are either actual values directly passed to the kernel or
  // references to memory.
  // OpenCL 2.0 pp 34: allowed address spaces
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
    llvm_unreachable("Impelent vector types");

  // It depends on the Instruction if values have to be interpreted as signed or
  // unsigned.
  Datatype Type = Invalid;

  switch ( I.getOpcode() ) {
    case Instruction::Add:
      Type = Signed;
      HWOp = makeHW<Add>(IVal, IName+"Add",Bits);
      break;
    case Instruction::FAdd:
      Type = Unsigned;
      HWOp = makeHW<FAdd>(IVal, IName+"FAdd",Bits);
      break;
    case Instruction::Sub:
      Type = Signed;
      HWOp = makeHW<Sub>(IVal,IName+"Sub",Bits);
      break;
    case Instruction::FSub:
      Type = Unsigned;
      HWOp = makeHW<FSub>(IVal,IName+"FSub",Bits);
      break;
    case Instruction::Mul:
      Type = Signed;
      HWOp = makeHW<Mul>(IVal,IName+"Mul",Bits);
      break;
    case Instruction::FMul:
      Type = Unsigned;
      HWOp = makeHW<FMul>(IVal,IName+"FMul",Bits);
      break;
    case Instruction::UDiv:
      Type = Unsigned;
      HWOp = makeHW<UDiv>(IVal,IName+"UDiv",Bits);
      break;
    case Instruction::SDiv:
      Type = Signed;
      HWOp = makeHW<SDiv>(IVal,IName+"SDiv",Bits);
      break;
    case Instruction::FDiv:
      Type = Unsigned;
      HWOp = makeHW<FDiv>(IVal,IName+"FDiv",Bits);
      break;
    case Instruction::URem:
      Type = Unsigned;
      HWOp = makeHW<URem>(IVal,IName+"URem",Bits);
      break;
    case Instruction::SRem:
      Type = Signed;
      HWOp = makeHW<SRem>(IVal,IName+"SRem",Bits);
      break;
    case Instruction::FRem:
      Type = Unsigned;
      HWOp = makeHW<FRem>(IVal,IName+"FRem",Bits);
      break;
      //Logical
    case Instruction::Shl:
      Type = Unsigned;
      HWOp = makeHW<Shl>(IVal,IName+"Shl",Bits);
      break;
    case Instruction::LShr:
      Type = Unsigned;
      HWOp = makeHW<LShr>(IVal,IName+"LShr",Bits);
      break;
    case Instruction::AShr:
      Type = Unsigned;
      HWOp = makeHW<AShr>(IVal,IName+"AShr",Bits);
      break;
    case Instruction::And:
      Type = Unsigned;
      HWOp = makeHW<And>(IVal,IName+"And",Bits);
      break;
    case Instruction::Or:
      Type = Unsigned;
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
      const_p HWConst = createConstant(ConstValue, I.getParent(), Type );

      connect(HWConst,HWOp);
    } else {
      base_p HWOperand = getHW<HW>(OpVal);

      connect(HWOperand,HWOp);
    }
  }
}

void OCLAccHWPass::visitLoadInst(LoadInst  &I)
{
  std::string Name = I.getName().str();
  Value *AddrVal = I.getPointerOperand();

  unsigned AddrSpace = I.getPointerAddressSpace();

  if ( AddrSpace != ocl::AS_GLOBAL && AddrSpace != ocl::AS_LOCAL )
    llvm_unreachable("NOT_IMPLEMENTED: Only global address space supported.");

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

  ValueMap[&I] = HWStreamIndex;

  HWStreamIndex->setName(HWStream->getName());
}

///
/// \brief Store Instructions perform a write access to memory. Multiple stores
/// to the same memory must preserve their order!
/// TODO: 1. Check Namespaces
/// 2. Address could be constant or Values
/// 3. If Value: Could be getElementPtrInst
/// 4. If getElementPtrInst: Connect Base and Index
/// 
void OCLAccHWPass::visitStoreInst(StoreInst &I)
{
  //errs() << __PRETTY_FUNCTION__ << "\n";

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
    const_p HWConst = createConstant(ConstValue, I.getParent(), Signed);
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
  } 
  else {
    llvm_unreachable("Index base address only streams.");
  }

  HWStream->addStore(HWStreamIndex);

  connect(HWData, HWStreamIndex);
  //StoreInst does not produce a HW Node since no value comes out of it
}

// Generate Offset only, Base will be handled by the actual load or store instruction.
void OCLAccHWPass::visitGetElementPtrInst(GetElementPtrInst &I)
{
  Value *InstValue = &I;
  Value *BaseValue = I.getPointerOperand();

  if (! I.isInBounds() )
    llvm_unreachable("Not in Bounds.");

  std::string Name = I.getName().str();

  //The Pointer base can either be a local Array or a input stream
  unsigned BaseAddressSpace = I.getPointerAddressSpace();
  if ( BaseAddressSpace != ocl::AS_GLOBAL )
    llvm_unreachable( "Only global address space supported." );

  ValueMapIt BaseIt = ValueMap.find(BaseValue);
  if (BaseIt == ValueMap.end())
    llvm_unreachable("Base not visited");

  //TODO local arrays with hierarchy to in/out
  streamport_p HWBase;
#if 0
  // currently no difference
  if (HWBase = std::dynamic_pointer_cast<OutStream>(BaseIt->second)) {
    //pass
  } else if (HWBase = std::dynamic_pointer_cast<InStream>(BaseIt->second)) {
    //pass
#endif
  if (! (HWBase = std::dynamic_pointer_cast<StreamPort>(BaseIt->second)))
    llvm_unreachable("Invalid getElementPtrInst Base Address");

  //Handle Index on the Base-Address
  if ( I.getNumIndices() != 1 )
    llvm_unreachable("Only 1D arrays supported");

  Value *IndexValue = *(I.idx_begin());

  base_p HWIndex;
  streamindex_p HWStreamIndex;

  // Create Constant or look for computed Index
  Constant *ConstValue = dyn_cast<Constant>(IndexValue);
  if (ConstValue) {
    Type *OpType = ConstValue->getType();

    if (!OpType->isIntegerTy())
      llvm_unreachable("No Integer Index");

    ConstantInt *IntConst = dyn_cast<ConstantInt>(ConstValue);
    if (!IntConst)
      llvm_unreachable("Unknown Constant Type");

    int64_t C = IntConst->getSExtValue();
    HWStreamIndex = std::make_shared<StaticStreamIndex>(Name, HWBase, C, IntConst->getValue().getActiveBits());
  } else {
    ValueMapIt IndexIt = ValueMap.find(IndexValue);
    if (IndexIt == ValueMap.end())
      llvm_unreachable("getElementPtrInst Index not Const and no HW-Mapping");

    HWIndex = IndexIt->second;
    HWStreamIndex = std::make_shared<DynamicStreamIndex>(Name, HWBase, HWIndex);

    //app Output but not input
    HWIndex->appOut(HWStreamIndex);
  }

  ValueMap[InstValue] = HWStreamIndex;

  return;
}

// Helper functions


/// \brief Create a Constant.
/// Do not use makeHW to create Constants. A single Constant objects might be used by
/// multiple instructions, leading to conflicts in the ValueMap.
///
/// FIXME Use actually required datatypes
const_p OCLAccHWPass::createConstant(Constant *C, BasicBlock *B, Datatype T) {
  std::stringstream CName;
  const_p HWConst;
  block_p HWBlock = getBlock(B);

  Type *CType = C->getType();

  if (ConstantInt *IConst = dyn_cast<ConstantInt>(C)) {
    switch (T) {
      case Signed:
        {
          int64_t S = IConst->getSExtValue();
          CName << S;
          HWConst = std::make_shared<ConstVal>(CName.str(), S, IConst->getBitWidth());
          break;
        }
      case Unsigned:
        {
          uint64_t U = IConst->getZExtValue();
          CName << U;
          HWConst = std::make_shared<ConstVal>(CName.str(), U, IConst->getBitWidth());
          break;
        }
      default:
        llvm_unreachable("Integer Constant must be Signed or Unsigned");
    }
  } 
  else if (ConstantFP *FConst = dyn_cast<ConstantFP>(C)) {

    const APFloat &Float = FConst->getValueAPF();

    if (CType->isFloatTy()) {
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
      llvm_unreachable("Unknown Constant Type");
  } else 
  {
    llvm_unreachable("Unsupported Constant Type");
  }

  HWConst->setBlock(HWBlock);
  HWBlock->addConstVal(HWConst);

  return HWConst;
}

/// \brief Check if the call is really a built-in
void OCLAccHWPass::visitCallInst(CallInst &I) {
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

char OCLAccHWPass::ID = 0;

static RegisterPass<OCLAccHWPass> X("oclacc-hw", "Create HW Tree.");

#ifdef TYPENAME
#undef TYPENAME
#endif
