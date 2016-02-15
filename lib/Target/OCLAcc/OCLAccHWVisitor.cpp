#include <sstream>
#include <memory>
#include <list>
#include <cxxabi.h>

#include "llvm/Support/raw_ostream.h"
#include "llvm/ADT/PostOrderIterator.h"
#include "llvm/IR/Dominators.h"
#include "llvm/IR/CFG.h"

#include "HW/HW.h"
#include "HW/typedefs.h"
#include "HW/Arith.h"
#include "HW/Constant.h"
#include "HW/Compare.h"
#include "HW/Control.h"
#include "HW/Kernel.h"
#include "HW/Memory.h"
#include "HW/Streams.h"
#include "HW/operators.cpp"

#include "OCLAccHWVisitor.h"
#include "OCLAccHWPass.h"
#include "OpenCLDefines.h"

#include "kernel_builtins.h"

#include "todo.h"


//
// creates ScalarInput/Output, if From and To are defined in different BBs.
//
void connect(base_p HWFrom, base_p HWTo) {
  block_p HWFromBB = HWFrom->getBlock();
  block_p HWToBB = HWTo->getBlock();

  if (HWFromBB && HWToBB
      && HWFromBB != HWToBB) {

    errs() << "Connecting HWs from different Blocks: From: " << HWFromBB->getName() << ", To: " << HWToBB->getName() << "\n";

    outscalar_p HWOutScalar = std::dynamic_pointer_cast<OutScalar>(HWFrom);
    inscalar_p HWInScalar = std::dynamic_pointer_cast<InScalar>(HWTo);

    if (!HWOutScalar) HWOutScalar = std::make_shared<OutScalar>(HWFrom->getName(), HWFrom->getBitwidth());
    if (!HWInScalar) HWInScalar = std::make_shared<InScalar>(HWTo->getName(), HWTo->getBitwidth());

    //add input/output to block
    HWFromBB->addOut(HWOutScalar);
    HWToBB->addIn(HWInScalar);

    HWFrom->appOut(HWOutScalar);
    HWOutScalar->appIn(HWFrom);

    HWTo->appIn(HWInScalar);
    HWInScalar->appOut(HWTo);

    HWOutScalar->appOut(HWInScalar);
    HWInScalar->appIn(HWOutScalar);
  } else {
    HWFrom->appOut(HWTo);
    HWTo->appIn(HWFrom);
  }
}

BasicBlock *OCLAccHWVisitor::getSingleSuccessor(BasicBlock* BB) {
  succ_iterator SI = succ_begin(BB), E = succ_end(BB);
  if (SI == E) return nullptr; // no successors
  BasicBlock *TheSucc = *SI;
  ++SI;
  return (SI == E) ? TheSucc : nullptr /* multiple successors */;
}

BasicBlock *OCLAccHWVisitor::getUniqueSuccessor(BasicBlock *BB) {
  succ_iterator SI = succ_begin(BB), E = succ_end(BB);
  if (SI == E) return NULL; // No successors
  BasicBlock *SuccBB = *SI;
  ++SI;
  for (;SI != E; ++SI) {
    if (*SI != SuccBB)
      return NULL;
  }
  return SuccBB;
}

size_t OCLAccHWVisitor::getScalarBitWidth(Value *V) {
  Type *T = V->getType();

  if (T->isIntegerTy()) {
    return T->getIntegerBitWidth();
  } else if (ConstantFP *FloatConst = dyn_cast<ConstantFP>(V)) {
    const APFloat &Float = FloatConst->getValueAPF();

    if (T->isFloatTy()) {
      return 32;
    } else if (T->isDoubleTy()) {
      return 64;
    } else
      llvm_unreachable("Unknown Floating Point Type");
  } else {
    errs() << V->getName() << ":\n";
    llvm_unreachable("Unknown Constant Type");
  }
}

//
//iterate over all operands = use-def -chain in doc 
//http://llvm.org/docs/ProgrammersManual.html#iterating-over-def-use-use-def-chains

/*
   errs() << "Binary Operator for Instruction " << InstVal->getName() << ": Operands ";
   for (auto &Op : I.operands() ) {
   errs() << " " << Op.getUser()->getName();
   }
   errs() << "\n";
   */


using namespace oclacc;

namespace llvm {

OCLAccHWVisitor::OCLAccHWVisitor(kernel_p K) : HWKernel(K) {
};

/*
 * Helper Functions
 */

const_p OCLAccHWVisitor::createConstant(Constant *C) {
  std::stringstream ConstName;
  const_p HWConst;

  Type *OpType = C->getType();

  if (OpType->isIntegerTy()) {
    ConstantInt *IntConst = dyn_cast<ConstantInt>(C);
    if (!IntConst)
      llvm_unreachable("Unknown Constant Type");

    uint64_t C = *(IntConst->getValue().getRawData());
    ConstName << IntConst->getSExtValue();
    HWConst = std::make_shared<ConstVal>(ConstName.str(),C, IntConst->getValue().getMinSignedBits());
    HWConst->setBlock(CurrentHWBlock);
    CurrentHWBlock->addConst(HWConst);


  } else if (OpType->isFloatingPointTy() ) {
    ConstantFP *FloatConst = dyn_cast<ConstantFP>(C);

    if (!FloatConst)
      llvm_unreachable("Unknown Constant Type");

    const APFloat &Float = FloatConst->getValueAPF();

    if (OpType->isFloatTy()) {
      float F = Float.convertToFloat();
      ConstName << F;
      const APInt Bits = Float.bitcastToAPInt();
      uint64_t C = *(Bits.getRawData());
      HWConst = std::make_shared<ConstVal>(ConstName.str(), C, Bits.getBitWidth());
      HWConst->setBlock(CurrentHWBlock);
      CurrentHWBlock->addConst(HWConst);


    } else if (OpType->isDoubleTy()) {
      double F = Float.convertToDouble();
      ConstName << F;
      const APInt Bits = Float.bitcastToAPInt();
      uint64_t C = *(Bits.getRawData());
      HWConst = std::make_shared<ConstVal>(ConstName.str(), C, Bits.getBitWidth());

      HWConst->setBlock(CurrentHWBlock);
      CurrentHWBlock->addConst(HWConst);
    } else
      llvm_unreachable("Unknown Constant Type");
  } else
    llvm_unreachable("Unknown Constant Type");

  return HWConst;
}

/*
 * Visit Functions
 */

 // void OCLAccHWVisitor::handlePredecessors(BasicBlock &I) {
 // }
 //
 /* Data structures:
  *
  * std::map<std::pair<BasicBlock *, BasicBlock *>, Value *> CondThenMap;
  * std::map<std::pair<BasicBlock *, BasicBlock *>, Value *> CondElseMap;
  */

block_p OCLAccHWVisitor::createHWBlock(BasicBlock* BB) {
  block_p HWBlock= std::make_shared<Block>(BB->getName());

  HWKernel->addBlock(HWBlock);

  BBMap[BB] = HWBlock;

  return HWBlock;
}

void OCLAccHWVisitor::visitBasicBlock(BasicBlock &I) {
  errs() << "VisitBasicBlock " << I.getName() << "\n";
  block_p HWBlock;

  BBMapIt BBIt = BBMap.find(&I);
  if (BBIt == BBMap.end())
    HWBlock = createHWBlock(&I);
  else
    HWBlock = BBIt->second;

  CurrentBB = &I;
  CurrentHWBlock = HWBlock;
}

void OCLAccHWVisitor::visitArgument(const Argument &I) {
#if 0
  Type * ArgType = I.getType();
  const Value *Val = &I;
  std::string Name = I.getName().str();

  if (ArgType->isPointerTy()) {
    bool isRead = false;
    bool isWritten = false;

    if ( I.onlyReadsMemory() ) {
      isRead = true;
    } else {
      // have to check the whole hierarchy
      std::list<const Value *> Values;
      Values.push_back(&I);

      while (! Values.empty()) {
        const Value *CurrVal = Values.back();
        Values.pop_back();

        if (const StoreInst *St = dyn_cast<StoreInst>(CurrVal)) {
          isWritten = true;
        } else if (const LoadInst *Ld = dyn_cast<LoadInst>(CurrVal)) {
          isRead = true;
        } else {
          for ( auto &Inst : CurrVal->uses() ) {
            Values.push_back(Inst.getUser());
          }
        }
      }
    }

    Type *ElementType   = ArgType->getPointerElementType();
    unsigned SizeInBits = ElementType->getScalarSizeInBits();

    unsigned AddressSpace = ArgType->getPointerAddressSpace();

    switch ( AddressSpace ) {
      case ocl::AS_LOCAL:
        llvm_unreachable("Local arguments not implemented.");
      case ocl::AS_GLOBAL:
        if ( ! isWritten ) {
          instream_p Stream = std::make_shared<InStream>(Name, SizeInBits);
          HWKernel->appInStream(Stream);
          ValueMap[&I] = Stream;
          errs() << Name << " is InStream of size " <<  SizeInBits << "\n";
        } else if ( ! isRead ) {
          outstream_p Stream = std::make_shared<OutStream>(Name, SizeInBits);
          HWKernel->appOutStream(Stream);
          ValueMap[&I] = Stream;
          errs() << Name << " is OutStream of size " <<  SizeInBits << "\n";
        } else {
          llvm_unreachable("No InOutStreams");
#if 0
          inoutstream_p Stream = std::make_shared<InOutStream>(Name, SizeInBits );
          HWKernel->appIn(Stream);
          HWKernel->appOut(Stream);
          ValueMap[&I] = Stream;
          errs() << Name << " is GlobalInOutStream of size " <<  SizeInBits << "\n";
#endif
        }
        break;
      default:
        errs() << "AddressSpace of "<< I.getName() << ": " << AddressSpace << "\n";
        llvm_unreachable( "AddressSpace not supported" );
    }
  } else if ( ArgType->isIntegerTy() ) {
    inscalar_p Scalar = std::make_shared<InScalar>(Name, ArgType->getScalarSizeInBits() );
    HWKernel->appInScalar(Scalar);
    ValueMap[&I] = Scalar;
    errs() << Name << " is InScalar (TODO int) of size " << ArgType->getScalarSizeInBits() << "\n";
  } else if ( ArgType->isFloatingPointTy() ) {
    inscalar_p Scalar = std::make_shared<InScalar>(Name, ArgType->getScalarSizeInBits() );
    HWKernel->appInScalar(Scalar);
    ValueMap[&I] = Scalar;
    errs() << Name << " is InScalar (TODO float) of size " << ArgType->getScalarSizeInBits() << "\n";
  } else {
    llvm_unreachable("Unknown Argument Type");
  }
#endif
}

void OCLAccHWVisitor::visitBinaryOperator(Instruction &I) {
  Value *InstVal = &I;
  Type *InstType = I.getType();

  if ( ValueMap.find(InstVal) != ValueMap.end() ) {
    errs() << InstVal->getName() << "\n";
    llvm_unreachable("already in ValueMap.");
  }

  std::string InstName = "["+InstVal->getName().str()+"]";

  base_p HWOp;

  size_t Bits = InstType->getPrimitiveSizeInBits();

  if (InstType->isVectorTy())
    TODO("Impelent vector types");

  switch ( I.getOpcode() ) {
    case Instruction::Add:
      HWOp = std::make_shared<Add>(InstName+"Add",Bits);
      break;
    case Instruction::FAdd:
      HWOp = std::make_shared<FAdd>(InstName+"FAdd",Bits);
      break;
    case Instruction::Sub:
      HWOp = std::make_shared<Sub>(InstName+"Sub",Bits);
      break;
    case Instruction::FSub:
      HWOp = std::make_shared<FSub>(InstName+"FSub",Bits);
      break;
    case Instruction::Mul:
      HWOp = std::make_shared<Mul>(InstName+"Mul",Bits);
      break;
    case Instruction::FMul:
      HWOp = std::make_shared<FMul>(InstName+"FMul",Bits);
      break;
    case Instruction::UDiv:
      HWOp = std::make_shared<UDiv>(InstName+"UDiv",Bits);
      break;
    case Instruction::SDiv:
      HWOp = std::make_shared<SDiv>(InstName+"SDiv",Bits);
      break;
    case Instruction::FDiv:
      HWOp = std::make_shared<FDiv>(InstName+"FDiv",Bits);
      break;
    case Instruction::URem:
      HWOp = std::make_shared<URem>(InstName+"URem",Bits);
      break;
    case Instruction::SRem:
      HWOp = std::make_shared<SRem>(InstName+"SRem",Bits);
      break;
    case Instruction::FRem:
      HWOp = std::make_shared<FRem>(InstName+"FRem",Bits);
      break;
      //Logical
    case Instruction::And:
      HWOp = std::make_shared<And>(InstName+"And",Bits);
      break;
    case Instruction::Or:
      HWOp = std::make_shared<Or>(InstName+"Or",Bits);
      break;
    default:
      errs() << "Unknown Binary Operator " << I.getOpcodeName() << "\n";
      llvm_unreachable("Halt.");
      return;
  }

  ValueMap[InstVal] = HWOp;
  CurrentHWBlock->addOp(HWOp);

  for (Value *OpVal : I.operand_values() ) {
    if (Constant *ConstValue = dyn_cast<Constant>(OpVal)) {
      const_p HWConst = createConstant(ConstValue);

      HWKernel->addConstVal(HWConst);
      connect(HWConst,HWOp);
    } else {
      ValueMapIt OpValIt = ValueMap.find(OpVal);

      if ( OpValIt == ValueMap.end() ) {
        errs() << "Ho HW-Object for " << OpVal->getName() << "\n";
        llvm_unreachable("Halt.");
      }

      base_p HWOperand = OpValIt->second;

      connect(HWOperand,HWOp);
    }
  }
}

void OCLAccHWVisitor::visitCmpInst(CmpInst &I)
{
  Value *CmpVal = &I;
  std::string Name = I.getName().str();

  if (I.isFPPredicate() ) {
    errs () << "Cmp Float\n";
  } else if (I.isIntPredicate()) {
    errs () << "Cmp Int\n";
  }

  base_p HWCmp;

  CmpInst::Predicate Pred = I.getPredicate();
  switch (Pred) {
    case CmpInst::ICMP_EQ:
      //errs() << "Integer EQ\n";
      Name += " IEQ";
      HWCmp = std::make_shared<Compare>(Name+" IEQ");
      break;
    case CmpInst::ICMP_NE:
      //errs() << "Integer NEQ\n";
      HWCmp = std::make_shared<Compare>(Name+" NE");
      break;
    case CmpInst::ICMP_SGT:
      //errs() << "Signed GT\n";
      HWCmp = std::make_shared<Compare>(Name+" SGT");
      break;
    case CmpInst::ICMP_SGE:
      //errs() << "Signed GE\n";
      HWCmp = std::make_shared<Compare>(Name+" SGE");
      break;
    case CmpInst::ICMP_SLT:
      //errs() << "Signed LT\n";
      HWCmp = std::make_shared<Compare>(Name+" SLT");
      break;
    case CmpInst::ICMP_SLE:
      //errs() << "Signed LE\n";
      HWCmp = std::make_shared<Compare>(Name+" SLE");
      break;
    default:
      errs() << "Compare Predicate: " << Pred << "\n";
      llvm_unreachable("Unsupported Compare-Type");
  }

  ValueMap[CmpVal] = HWCmp;

  for (auto &Op : I.operands() ) {
    Value *OpVal = Op.get();

    Constant *ConstValue = dyn_cast<Constant>(OpVal);
    if (!ConstValue) {
      ValueMapIt OpValIt = ValueMap.find(OpVal);

      if ( OpValIt == ValueMap.end() ) {
        errs() << "Ho HW-Object for " << OpVal->getName() << "\n";
        llvm_unreachable("Halt.");
      } else {
        base_p HWOperand = OpValIt->second;
        connect(HWOperand, HWCmp);
      }
    } else {
      const_p HWConst = createConstant(ConstValue);
      HWKernel->addConstVal(HWConst);
      connect(HWConst,HWCmp);
    }
  }
}

void OCLAccHWVisitor::visitTruncInst(TruncInst &I)
{
  base_p HWTmp = std::make_shared<Tmp>( I.getName() );
  ValueMap[&I] = HWTmp;

  //link operands
  for ( auto &Op : I.operands() ) {
    Value *OpVal = Op.get();

    ValueMapIt OpValIt = ValueMap.find(OpVal);

    if ( OpValIt == ValueMap.end() ) {
      errs() << I << "\n";
      llvm_unreachable("Operand not visited");
    }

    base_p HWOp = OpValIt->second;

    connect(HWOp,HWTmp);
  }
}

#if 0
  //base_p HWTmp = std::make_shared<Tmp>( I.getName().str() );
  HWIn > HWTmp;
  ValueMap[&I] = HWTmp;
#endif

void OCLAccHWVisitor::handleBuiltinWorkGroupBarrier( CallInst &I, const std::string &FunctionName ) {
  //HWBarrier = std::make_shared<WorkGroupBarrier> 

  if ( I.getNumArgOperands() != 2 )
    llvm_unreachable( "Only two Operands suppoorted by OpenCL-Builtin WorkGroupBarrier" );

  /* 
   * Handle Fence argument 
   */
  Value *OpFence = I.getArgOperand(0);

  ConstantInt *IntFence = dyn_cast<ConstantInt>(OpFence);
  if ( ! IntFence ) 
    llvm_unreachable( "Only ConstantInt Operands for OpenCL-Builtin Functions supported." );

  uint64_t ValFence = IntFence->getSExtValue();

  if ( ValFence & CLK_LOCAL_MEM_FENCE ) {
  }

  if ( ValFence & CLK_GLOBAL_MEM_FENCE ) {
  }

  if ( ValFence & CLK_IMAGE_MEM_FENCE ) {
  }

  /* 
   * Handle Scope 
   */

  Value *OpScope = I.getArgOperand(1);

  ConstantInt *IntScope = dyn_cast<ConstantInt>(OpScope);
  if ( ! IntScope ) 
    llvm_unreachable( "Only ConstantInt Operands for OpenCL-Builtin Functions supported." );

  uint64_t ValScope = IntScope->getSExtValue();

  switch ( ValScope ) {
    case memory_scope_work_item:
      break;
    case memory_scope_work_group:
      break;
    case memory_scope_device:
      break;
    case memory_scope_all_svm_devices:
      break;
    default:
      llvm_unreachable( "Invalid Scope for OpenCL-Buildin WorkGroupBarrier" );
  } 
}

void OCLAccHWVisitor::handleBuiltinMath(CallInst &I, const std::string &FunctionName)
{
}
void OCLAccHWVisitor::handleBuiltinInteger(CallInst &I, const std::string &FunctionName)
{
}
void OCLAccHWVisitor::handleBuiltinToGlobal(CallInst &I, const std::string &FunctionName)
{
}
void OCLAccHWVisitor::handleBuiltinToLocal(CallInst &I, const std::string &FunctionName)
{
}
void OCLAccHWVisitor::handleBuiltinToPrivate(CallInst &I, const std::string &FunctionName)
{
}
void OCLAccHWVisitor::handleBuiltinGetFence(CallInst &I, const std::string &FunctionName)
{
}
void OCLAccHWVisitor::handleBuiltinPrintf(CallInst &I, const std::string &FunctionName)
{

}


/// \brief Handle built-in function calls.
///
/// 
/// Calls to built-in work-item and calls to helper functions must have been 
/// replaced before.
///
/// Demangled function names taken from
/// https://github.com/KhronosGroup/SPIR-Tools/wiki/SPIR-2.0-built-in-functions
/// 
/// FIXME optionally implement mangeling
///
void
OCLAccHWVisitor::visitCallInst(CallInst &I)
{
  Value *CallVal = &I;
  const Value *Callee = I.getCalledValue();
  std::string CalleeName = Callee->getName().str();

  // Look for work-item functions
  if (std::find(ocl::FUN_WI_M_B, ocl::FUN_WI_M_E, CalleeName) != ocl::FUN_WI_M_E) 
  {
    llvm_unreachable("All calls to work-item functions must be eliminated");
  }

  // Math Functions
  auto MI = std::find(ocl::FUN_MATH_M_B, ocl::FUN_MATH_M_E, CalleeName);
  if (MI != ocl::FUN_MATH_M_E) {
    int i = std::distance(ocl::FUN_MATH_M_B, MI);
    llvm_unreachable("unimplemented math function");
  }

  // Integer Functions
  auto II = std::find(ocl::FUN_INT_M_B, ocl::FUN_INT_M_E, CalleeName);
  if (MI != ocl::FUN_INT_M_E) {
    int i = std::distance(ocl::FUN_INT_M_B, MI);
    llvm_unreachable("unimplemented integer function");
  }

  /* Common Functions */
  /* Geometric Functions */
  /* Relational Functions */
  /* Vector Load and Store Functions */

  /* Synchronization Functions */
  else if ( CalleeName == "work_group_barrier" ) {
    handleBuiltinWorkGroupBarrier(I, CalleeName);
  }

  /* Address Space Qualifier Functions */
  else if ( CalleeName == "to_global" ) {
    handleBuiltinToGlobal(I, CalleeName);
  } else if ( CalleeName == "to_local" ) {
    handleBuiltinToLocal(I, CalleeName);
  } else if ( CalleeName == "to_private" ) {
    handleBuiltinToPrivate(I, CalleeName);
  } else if ( CalleeName == "get_fence" ) {
    handleBuiltinGetFence(I, CalleeName);
  }

  /* Async Copies from Global->Local, Local->Global, Prefetch */
  /* Atomic Functions */
  /* Miscellaneous Vector Functions */

  /* printf */
  else if ( CalleeName == "printf" ) {
    handleBuiltinPrintf(I, CalleeName);
  }

  /* LLVM Functions */
  else if ( CalleeName == "llvm.dbg.value" ) {
    //nothing 
  }

  else {
    errs() << "Function " << CalleeName << "\n";
    llvm_unreachable( "Only OpenCL-Builtin Functions supported." );
  }
}

void OCLAccHWVisitor::visitReturnInst(ReturnInst &I)
{
  if ( I.getReturnValue() )
    llvm_unreachable("NOT_IMPLEMENTED: Only void kernels supported.");
}

void OCLAccHWVisitor::visitBranchInst(BranchInst &I)
{
  BasicBlock *FirstBB = I.getSuccessor(0);

  block_p HWFirstBlock;
  BBMapIt BBIt = BBMap.find(FirstBB);
  if (BBIt == BBMap.end())
    HWFirstBlock = createHWBlock(FirstBB);
  else
   HWFirstBlock = BBIt->second;

  if (I.isConditional()) {
    Value *Cond = I.getCondition();

    BasicBlock *TrueBB = FirstBB;
    BasicBlock *FalseBB = I.getSuccessor(1);

    block_p HWSecondBlock;
    BBIt = BBMap.find(FalseBB);
    if (BBIt == BBMap.end())
      HWSecondBlock = createHWBlock(FalseBB);
    else
      HWSecondBlock = BBIt->second;


    CondMap[std::make_pair(CurrentBB, TrueBB)] = std::make_pair(Cond, COND_TRUE);
    CondMap[std::make_pair(CurrentBB, FalseBB)] = std::make_pair(Cond, COND_FALSE);

    // Condition has to be output to be able to generate multiplexers
    ValueMapIt CondIt = ValueMap.find(Cond);
    if (CondIt == ValueMap.end())
      llvm_unreachable("Condition Value not visited yet");

    base_p HWCond = CondIt->second;

    outscalar_p HWOut = std::make_shared<OutScalar>(Cond->getName(), getScalarBitWidth(Cond));
    connect(HWCond, HWOut);

    //
    //set leaving conditions in block
    //
    CurrentHWBlock->setCondition(HWCond);
    CurrentHWBlock->addSuccessor(HWFirstBlock, COND_TRUE);
    CurrentHWBlock->addSuccessor(HWSecondBlock, COND_FALSE);

  } else {
    UncondSet.insert(std::make_pair(CurrentBB, FirstBB));


    CurrentHWBlock->setCondition(nullptr);
    CurrentHWBlock->addSuccessor(HWFirstBlock, COND_NONE);
  }
}

void OCLAccHWVisitor::visitSwitchInst(SwitchInst &I) {
  llvm_unreachable("Switch not supported.");
}

void OCLAccHWVisitor::visitInsertElementInst(InsertElementInst &I)
{
}

void OCLAccHWVisitor::visitLoadInst(LoadInst  &I)
{
  std::string Name = I.getName().str();
  Value *AddrVal = I.getPointerOperand();

  unsigned AddrSpace = I.getPointerAddressSpace();

  if ( AddrSpace != ocl::AS_GLOBAL && AddrSpace != ocl::AS_LOCAL )
    llvm_unreachable("NOT_IMPLEMENTED: Only global address space supported.");

  //Get Address to store at
  ValueMapIt AddrIt = ValueMap.find( AddrVal );
  if ( AddrIt == ValueMap.end() )
    llvm_unreachable("Load Address not Visited.");

  streamindex_p HWStreamIndex = std::dynamic_pointer_cast<StreamIndex>(AddrIt->second);
  if (!HWStreamIndex) {
    llvm_unreachable("Index base address only streams.");
  }

  // get the stream to read from
  stream_p HWStream = HWStreamIndex->getStream();
  //TODO This is the case for local arrays!
  if (! HWStream)
    llvm_unreachable("Load base address is not a stream");

  HWStream->appIndex(HWStreamIndex);

  ValueMap[&I] = HWStreamIndex;

  HWStreamIndex->setName(HWStream->getName());
}

/*
 * Store Instructions perform a write access to memory.
 * TODO: 1. Check Namespaces
 * 2. Address could be constant or Values
 * 3. If Value: Could be getElementPtrInst
 * 4. If getElementPtrInst: Connect Base and Index
 */
void OCLAccHWVisitor::visitStoreInst(StoreInst &I)
{
  //errs() << __PRETTY_FUNCTION__ << "\n";

  const std::string Name = I.getName().str();

  Value *DataVal = I.getValueOperand();
  Value *AddrVal = I.getPointerOperand();

  // Check Address Space
  unsigned AddrSpace = I.getPointerAddressSpace();

  if ( AddrSpace != ocl::AS_GLOBAL && AddrSpace != ocl::AS_LOCAL )
    llvm_unreachable("NOT_IMPLEMENTED: Only global and local address space supported.");

  base_p HWData;
  base_p HWOut;

  //Get Data to store
  ValueMapIt DataIt = ValueMap.find(DataVal);
  if ( DataIt != ValueMap.end() ) {
    HWData = DataIt->second;
  } else {
    if ( Constant *ConstValue = dyn_cast<Constant>(DataVal) ) {
      const_p HWConst = createConstant(ConstValue);
      HWKernel->addConstVal(HWConst);
      HWData = HWConst;
    } else {
      errs() << DataVal->getName() << "\n";
      llvm_unreachable( "Store Data not visited and not constant");
    }
  }

  //Get Address to store at
  ValueMapIt AddrIt = ValueMap.find(AddrVal);
  if (AddrIt == ValueMap.end() )
    llvm_unreachable("Store Address not Visited.");

  stream_p HWStream;
  streamindex_p HWStreamIndex;

  if ( (HWStreamIndex = std::dynamic_pointer_cast<StreamIndex>(AddrIt->second)) ) {
    HWStream = HWStreamIndex->getStream();
  } else if ((HWStream = std::dynamic_pointer_cast<Stream>(AddrIt->second))) {
    HWStreamIndex = std::make_shared<StaticStreamIndex>("0", HWStream, 0, 1);
  } else {
    llvm_unreachable("Index base address only streams.");
  }

  HWStream->appIndex(HWStreamIndex);

  connect(HWData, HWStreamIndex);
  //StoreInst does not produce a HW Node since no value comes out of it

  HWStreamIndex->setName(HWStream->getName());
}

// Generate Offset only, Base will be handled by the actual load or store instruction.
void OCLAccHWVisitor::visitGetElementPtrInst(GetElementPtrInst &I)
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
  stream_p HWBase;
#if 0
  // currently no difference
  if (HWBase = std::dynamic_pointer_cast<OutStream>(BaseIt->second)) {
    //pass
  } else if (HWBase = std::dynamic_pointer_cast<InStream>(BaseIt->second)) {
    //pass
#endif
  if (! (HWBase = std::dynamic_pointer_cast<Stream>(BaseIt->second)))
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

    uint64_t C = *(IntConst->getValue().getRawData());
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

/**
 * Walk all incoming blocks 
 *
 * Each block stores information, how control flow can diverge leaving the
 * block, but does not know which Values are used outside by PHIs.
 */
void OCLAccHWVisitor::visitPHINode( PHINode &I) {
  mux_p HWMux = std::make_shared<Mux>(I.getName());
  HWMux->setBlock(CurrentHWBlock);


  for (PHINode::block_iterator BI=I.block_begin(), E=I.block_end(); BI != E; ++BI) {
    BasicBlock *ThisBB = *BI;
    BasicBlock *ThatBB = *BI;
    Value *V = I.getIncomingValueForBlock(ThatBB);

    // Instruction needed for getParent();
    Instruction *VI = dyn_cast<Instruction>(V);
    if (! VI) {
      errs() << "Incoming value not generated by an instruction: " << V->getName() << "\n";
      llvm_unreachable("No\n");
    }

    // Get block where value was defined.
    // May be different from *BI
    BasicBlock *ValBB = VI->getParent();

    //Find HW that generates the Value
    ValueMapIt ValIt = ValueMap.find(V);
    if (ValIt == ValueMap.end())
      llvm_unreachable("HW used as block input which has not been generated yet");

    base_p HWValue = ValIt->second;


    //Find corresponding HWBlock
    BBMapIt ValBBIt = BBMap.find(ValBB);
    if (ValBBIt == BBMap.end())
      llvm_unreachable("No mapping for BB to HWBlock");

    block_p HWBlock = ValBBIt->second;




    // If ThatBB and ValBB differ, they have to be connected.
    if (ValBB != ThisBB) {
      errs() << V->getName() << " defined in " << ValBB->getName() << ", not in " << ThisBB->getName() << "\n";
    }

    //outscalar_p HWOut = std::make_shared<OutScalar>( HWValue->getName(), HWValue->getBitwidth() );

    inscalar_p HWIn = std::make_shared<InScalar>( HWValue->getName(), HWValue->getBitwidth() );
    HWIn->setBlock(CurrentHWBlock);
    CurrentHWBlock->addIn(HWIn);

    HWIn->appOut(HWMux);
    HWMux->addMuxIn(HWIn, HWBlock);


    //connect(HWValue, HWMux);
  }
  ValueMap[&I] = HWMux;
}

#if 0

    //Find HW that generates the Value
    ValueMapIt ValIt = ValueMap.find(V);
    if (ValIt == ValueMap.end())
      llvm_unreachable("HW used as block input which has not been generated yet");

    base_p HWValue = ValIt->second;

    //Find corresponding HWBlock
    BBMapIt ValBBIt = BBMap.find(ValBB);
    if (ValBBIt == BBMap.end())
      llvm_unreachable("No mapping for BB to HWBlock");

    block_p HWValBB = ValBBIt->second;

    // Move backward in control flow to find a way from the definition of a
    // value to its use inside this PHINode.
    // 
    // Start -> ThattBB -> ThisBB -> Phi containing BB
    //   

    inscalar_p HWIn;
    outscalar_p HWOut;
    block_p HWThisBB;
    block_p HWThatBB;

    errs() << "PHINode: Value: " << V->getName() << ", start in Block: " << ThisBB->getName() << ", def in Block: " << ValBB->getName() << "\n";

    for (pred_iterator IT = pred_begin(ThisBB), IE = pred_end(ValBB); IT != IE; ++IT) {
      ThatBB = *IT;

      // is transition from ThatBB to ThisBB depending on a condition?

      errs() << ThatBB->getName() << " to " << ThisBB->getName() << " ";
      //Unconditional
      UncondSetIt IsNotCondIt = UncondSet.find(std::make_pair(ThatBB, ThisBB));
      if (IsNotCondIt != UncondSet.end()) {
        errs() << "unconditional\n";

      } else {
        CondMapIt IsCondIt = CondMap.find(std::make_pair(ThatBB, ThisBB));
        if (IsCondIt == CondMap.end())
          llvm_unreachable("No BB transition");

        Value *Cond = IsCondIt->second.first;
        ValueMapIt CondIt = ValueMap.find(Cond);
        if (CondIt == ValueMap.end())
          llvm_unreachable("Condition not found");

        base_p HWCond = CondIt->second;
        ConditionFlag Flag = IsCondIt->second.second;

       // Inputs.push_back(std::make_pair(HWCond, Flag));

        errs() << "cond " << Cond->getName() << " ";

        if (Flag == COND_TRUE) {
          errs() << "true\n";
        } else if (Flag == COND_FALSE) {
          errs() << "false\n";
        } else
          llvm_unreachable("No Flag for Condition");
      }

      BBMapIt ValBBIt = BBMap.find(ThisBB);
      if (ValBBIt == BBMap.end())
        llvm_unreachable("No mapping for ThisBB to HWBlock");

      block_p HWThisBB = ValBBIt->second;

      ValBBIt = BBMap.find(ThatBB);
      if (ValBBIt == BBMap.end())
        llvm_unreachable("No mapping for ThatBB to HWBlock");

      block_p HWThatBB = ValBBIt->second;


      // List of conditions needed to generate Multiplexer. A single Muxer only
      // has a single condition.

      HWOut = std::make_shared<OutScalar>(I.getName(), 0);
      HWIn = std::make_shared<InScalar>(I.getName(), 0);

      HWThatBB->addOut(HWOut);
      HWThisBB->addIn(HWIn);

      connect(HWOut, HWIn);

      ThisBB = ThatBB;
    }


  }

  inscalar_p HWInScalar = std::make_shared<InScalar>(I.getName(), 0);

  ValueMap[&I] = HWInScalar;
}
#endif

#if 0
std::string Name = I.getName().str();

base_p HWPhi = std::make_shared<Tmp>( Name );
ValueMap[&I] = HWPhi;

//Clear all old Multiplexer-Instances
HWThenMap.clear();
HWElseMap.clear();

//According to http://llvm.org/docs/doxygen/html/PostOrderIterator_8h_source.html,
//it is expensive to create post-order-traversal. Thus it is done once before the loop.

base_p HWPred;

for (unsigned i = 0; i < I.getNumIncomingValues(); ++i ) {
  Value *InVal        = I.getIncomingValue(i);
  std::string Name = InVal->getName();

  BasicBlock *FromBlock = I.getIncomingBlock(i);
  BasicBlock *TargetBlock = I.getParent();

  //errs() << "PHINode " << Name << " Incoming Name: " << Name << ", From: " << FromBlock->getName() << " Target: " << TargetBlock->getName() << "\n";

  //traverse all the way back to the entry of the function to find the combination of all branch instructions leading
  //to the current From-Target-Combination. By that way, the computed values are multiplexed.

  ValueMapIt ValIt = ValueMap.find(InVal);
  if ( ValIt == ValueMap.end() ) {
    errs() << "PHI InVal not visited: " << InVal->getName() << "\n";
    llvm_unreachable("Halt.");
  }

  HWPred = ValIt->second;

  bool found = false;
  for (
      po_iterator<BasicBlock *> IT = po_begin(&KernelFunction->getEntryBlock()), IE = po_end(TargetBlock),
      PREVIT = ++po_begin(&KernelFunction->getEntryBlock()); PREVIT != IE; ++IT, ++PREVIT) 
  {

    BasicBlock *CurrBlock = *PREVIT;
    BasicBlock *NextBlock = *IT;

    if( FromBlock == CurrBlock ) {
      found = true;
    };


    if ( found ) {
      //errs() << "\tCurr:  " << (*IT)->getName() << " Next: " << (*PREVIT)->getName() << "\n";

      //Check if transition from FromBlock to TargetBlock is unconditional or conditional.
      UncondSetIt SetIt = UncondSet.find(std::make_pair(CurrBlock, NextBlock) );
      if (SetIt != UncondSet.end() )  {
          //errs() << "\tUnconditional Branch from " << CurrBlock->getName() << " to " << NextBlock->getName() << "\n";
          std::stringstream Name;
          Name << CurrBlock->getName().str() << "->" << NextBlock->getName().str();

          //This tmp-val should not be necessary.
          //base_p HWTmp(new Tmp( Name.str() ) );
          //
          //HWPred > HWTmp;
          //HWPred = HWTmp;

          continue;
        }

        CondMapIt MapIt = CondThenMap.find(std::make_pair(CurrBlock, NextBlock));
        if ( MapIt != CondThenMap.end() ) {
          Value *Cond = MapIt->second;

          ValueMapIt CondIt = ValueMap.find(Cond);
          if ( CondIt == ValueMap.end() ) {
            llvm_unreachable("Halt.");
          }
          base_p HWCond = CondIt->second;

          std::stringstream Name;
          Name << CurrBlock->getName().str() << "->" << NextBlock->getName().str();

          base_p HWMux;

          //check if multiplexer already available
          HWMapIt MuxIt = HWThenMap.find(std::make_pair(CurrBlock, NextBlock));
          if ( MuxIt != HWThenMap.end() ) {
             HWMux = MuxIt->second;
          } else {
            MuxIt = HWElseMap.find(std::make_pair(CurrBlock, NextBlock));
            if ( MuxIt != HWElseMap.end() ) {
              HWMux = MuxIt->second;
            } else {
              HWMux = std::make_shared<Mux>( Name.str(), HWCond ) ;
              HWThenMap[std::make_pair(CurrBlock, NextBlock)] = HWMux;
            }
          }

          connect(HWPred,HWMux);

          HWPred = HWMux;

          //errs() << "\tCondThenMap Entry for Conditional " << Cond->getName() << " From: " << CurrBlock->getName() << " To: " << NextBlock->getName() << "\n";

          continue;
        }

        MapIt = CondElseMap.find(std::make_pair(CurrBlock, NextBlock));
        if ( MapIt != CondElseMap.end() ) {
          Value *Cond = MapIt->second;
          TODO("");

          //errs() << "\tCondThenMap Entry for Conditional " << Cond->getName() << " From: " << CurrBlock->getName() << " To: " << NextBlock->getName() << "\n";

          continue;
        }

        llvm_unreachable("Halt.");
      }
    }

    connect(HWPred,HWPhi);

      //TODO
    //HWKernel->addBlock(HWPhi);
  }
#endif

} //end namespace llvm
