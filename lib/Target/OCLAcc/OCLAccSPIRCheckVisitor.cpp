#include <sstream>
#include <memory>

#include "llvm/Support/raw_ostream.h"
#include "llvm/ADT/PostOrderIterator.h"
#include "llvm/IR/Dominators.h"

#include "OCLAccSPIRCheckVisitor.h"
#include "OCLAccHW.h"
#include "OpenCLDefines.h"

#include "kernel_builtins.h"

#include "macros.h"

using namespace oclacc;

namespace llvm {

OCLAccSPIRCheckVisitor::OCLAccSPIRCheckVisitor() {

};

void OCLAccSPIRCheckVisitor::extractKernelInfo(const Module &M) {
  /*
   * Find OpenCL-Kernels
   */
  NamedMDNode *KernelList = M.getNamedMetadata("opencl.kernels");
  if ( ! KernelList ) {
    llvm_unreachable("No Kernels found");
  }
  for ( const MDNode *Node: KernelList->operands() ) {
    unsigned Count = Node->getNumOperands();

    if ( Count < 1 )
      llvm_unreachable("Kernel has no operands" );

    /*
     * Obligatory function
     */
    Function *Kernel = mdconst::dyn_extract<Function>(Node->getOperand(0));
    if ( ! Kernel )
      llvm_unreachable( "Kernel is no function" );

    std::string KernelName = Kernel->getName();

    if ( ! KernelName.length() )
      llvm_unreachable("Kernel has no name");

    KernelMap[KernelName] = Kernel;

    /*
     * Optional attributes
     */
    for (unsigned i=1; i < Count; ++i) {
      MDNode *Op = dyn_cast_or_null<MDNode>(Node->getOperand(i).get());

      if (!Op) llvm_unreachable("Invalid Metadata");

      unsigned Count = Op->getNumOperands();

      if (Count < 1) 
        llvm_unreachable("Invalid Metadata");

      MDString *MName = dyn_cast_or_null<MDString>(Op->getOperand(0));
      if (!MName) 
        llvm_unreachable("Invalid Metadata");

      const std::string &Name = MName->getString();

      if (Name.compare("work_group_size_hint") == 0) {
        if (Count != 4)
          llvm_unreachable("Invalid Metadata");

        ConstantInt *X = mdconst::dyn_extract<ConstantInt>(Op->getOperand(1));
        ConstantInt *Y = mdconst::dyn_extract<ConstantInt>(Op->getOperand(2));
        ConstantInt *Z = mdconst::dyn_extract<ConstantInt>(Op->getOperand(3));

        if (X->getBitWidth() != 32)
          NonConformingExpr.push_back(std::to_string(__LINE__)+": WorkGroupSizeHint invalid");

        WorkGroupSizeHint.X = X->getSExtValue();
        WorkGroupSizeHint.Y = Y->getSExtValue();
        WorkGroupSizeHint.Z = Z->getSExtValue();

      }
      else if (Name.compare("reqd_work_group_size") == 0) {
        if (Count != 4)
          llvm_unreachable("Invalid Metadata");

        ConstantInt *X = mdconst::dyn_extract<ConstantInt>(Op->getOperand(1));
        ConstantInt *Y = mdconst::dyn_extract<ConstantInt>(Op->getOperand(2));
        ConstantInt *Z = mdconst::dyn_extract<ConstantInt>(Op->getOperand(3));
        /* TODO Check for i32 Constants */

        ReqdWorkGroupSize.X = X->getSExtValue();
        ReqdWorkGroupSize.Y = Y->getSExtValue();
        ReqdWorkGroupSize.Z = Z->getSExtValue();
      }
      else if (Name.compare("vec_type_hint") == 0) {
        if (Count != 3)
          llvm_unreachable("Invalid Metadata");

        ConstantInt *X = mdconst::dyn_extract<ConstantInt>(Op->getOperand(1));
        ConstantInt *S = mdconst::dyn_extract<ConstantInt>(Op->getOperand(2));

        bool Signed = S->isOne();
      } else if (Name.compare("kernel_arg_addr_space") == 0) {
      } else if (Name.compare("kernel_arg_access_qual") == 0) {
      } else if (Name.compare("kernel_arg_optional_qual") == 0) {
      } else if (Name.compare("kernel_arg_base_type") == 0) {
      } else if (Name.compare("kernel_arg_type_qual") == 0) {
      } else if (Name.compare("kernel_arg_name") == 0) {
      } else if (Name.compare("kernel_arg_type") == 0) {
      } else
        MName->dump();
    }
  }
}

void OCLAccSPIRCheckVisitor::visitReturnInst(ReturnInst &I) {
  if ( I.getReturnValue() )
    llvm_unreachable("NOT_IMPLEMENTED: Only void kernels supported.");
}

void OCLAccSPIRCheckVisitor::visitModule(Module &M) {
  /*
   * check Target Triple
   */
  const std::string &TT = M.getTargetTriple();
  if (TT.compare("spir-unknown-unknown") == 0) {
    _64Bit = false;
  } else if (TT.compare("spir64-unknown-unknown") == 0) {
    _64Bit = true;
  } else llvm_unreachable("Invalid Target Triple");

  /*
   * extract ModuleName
   */
  ModuleName = M.getModuleIdentifier();
  if ( ModuleName.length() == 0 ||
      ModuleName.compare("<stdin>") == 0 )
    llvm_unreachable("ModuleName invalid");

  int LastDot = ModuleName.find_last_of(".");
  ModuleName  = ModuleName.substr(0, LastDot);

  /*
   * Check Data Layout.
   */
  const std::string &L = M.getDataLayoutStr();
  if (L.compare("e-p:32:32:32-i1:8:8-i8:8:8-i16:16:16-i32:32:32-i64:64:64-\
        f32:32:32-f64:64:64-v16:16:16-v24:32:32-v32:32:32-v48:64:64-\
        v64:64:64-v96:128:128-v128:128:128-v192:256:256-v256:256:256-\
        v512:512:512-v1024:1024:1024") != 0 
      && L.compare("e-p:64:64:64-i1:8:8-i8:8:8-i16:16:16-i32:32:32-i64:64:64-\
        f32:32:32-f64:64:64-v16:16:16-v24:32:32-v32:32:32-v48:64:64-\
        v64:64:64-v96:128:128-v128:128:128-v192:256:256-v256:256:256-\
        v512:512:512-v1024:1024:1024") != 0
      )
  {
    // FIXME: Data Layout conform to SPIR v2.0 but currently invalid in Clang 3.6
    NonConformingExpr.push_back("LLVM Target layout does not conform to SPIR:\n" + L);
  };

  extractKernelInfo(M);
}
void OCLAccSPIRCheckVisitor::visitFunction(Function &F) {

}
void OCLAccSPIRCheckVisitor::visitBasicBlock(BasicBlock &BB) {

}
void OCLAccSPIRCheckVisitor::visitInstruction(Instruction &I) { llvm_unreachable("not supported"); }

void OCLAccSPIRCheckVisitor::visitBranchInst(BranchInst &I) {
  BasicBlock *From = I.getParent();

  if ( I.getNumSuccessors() > 2 )
    llvm_unreachable("Too many successors");

  if ( I.getNumSuccessors() > 0 ) {
    BasicBlock *Then = I.getSuccessor(0);

    if ( I.isUnconditional() ) {
      //pass
    } else {
      if ( I.getNumSuccessors() > 1 ) {
        BasicBlock *Else = I.getSuccessor(1);
      }
    }
  }

  for (unsigned i = 0; i < I.getNumSuccessors(); ++i) {
    errsline() << "Visit branch successor " << i << ": " << I.getSuccessor(i)->getName() << "\n";
  }
}

void OCLAccSPIRCheckVisitor::visitSwitchInst(SwitchInst &I) {
}

void OCLAccSPIRCheckVisitor::visitUnreachableInst(UnreachableInst &I) {
}

void OCLAccSPIRCheckVisitor::visitBinaryOperator(Instruction &I) {

  std::string Name = I.getOpcodeName();

  Value *InstVal = &I;

  std::string InstName = InstVal->getName().str();

  switch ( I.getOpcode() ) {
    case Instruction::Add:
      break;
    case Instruction::FAdd:
      break;
    case Instruction::Sub:
      break;
    case Instruction::FSub:
      break;
    case Instruction::Mul:
      break;
    case Instruction::FMul:
      break;
    case Instruction::UDiv:
      break;
    case Instruction::SDiv:
      break;
    case Instruction::FDiv:
      break;
    case Instruction::URem:
      break;
    case Instruction::SRem:
      break;
    case Instruction::FRem:
      break;
    case Instruction::Shl:
      break;
    case Instruction::LShr:
      break;
    case Instruction::AShr:
      break;
      //Logical
    case Instruction::And:
      break;
    case Instruction::Or:
      break;
    case Instruction::Xor:
      break;
    default:
      errs() << "Unknown Binary Operator " << Name << "\n";
      llvm_unreachable("Halt.");
      return;
  }
}

void OCLAccSPIRCheckVisitor::visitExtractElementInst(ExtractElementInst &I) {
}

void OCLAccSPIRCheckVisitor::visitInsertElementInst(InsertElementInst &I) {
}

void OCLAccSPIRCheckVisitor::visitShuffleVectorInst(ShuffleVectorInst &I) {
}

void OCLAccSPIRCheckVisitor::visitExtractValueInst(ExtractValueInst &I) {
}

void OCLAccSPIRCheckVisitor::visitInsertValueInst(InsertValueInst &I) {
}


void OCLAccSPIRCheckVisitor::visitAllocaInst(AllocaInst &I) {}

void OCLAccSPIRCheckVisitor::visitLoadInst(LoadInst &I) {
  std::string Name = I.getName().str();
  Value *AddrVal    = I.getPointerOperand();

  unsigned AddrSpace = I.getPointerAddressSpace();

  if ( AddrSpace != ocl::AS_GLOBAL && AddrSpace != ocl::AS_LOCAL )
    llvm_unreachable("NOT_IMPLEMENTED: Only global address space supported.");
 
  if ( GetElementPtrInst *TargetInst = dyn_cast<GetElementPtrInst>(AddrVal) ) {
    Value *TargetBase = TargetInst->getPointerOperand();
  } else {
  }
}

/* 
 * Stores may not be atomic
 */
void OCLAccSPIRCheckVisitor::visitStoreInst(StoreInst &I) {
  
  std::string Name = I.getName().str();
  Value *InstVal = &I;
  Value *DataVal = I.getValueOperand();
  Value *AddrVal = I.getPointerOperand();

  unsigned AddrSpace = I.getPointerAddressSpace();

  if ( AddrSpace != ocl::AS_GLOBAL && AddrSpace != ocl::AS_LOCAL )
    llvm_unreachable("NOT_IMPLEMENTED: Only global address space supported.");

  if (I.isAtomic())
    NonConformingExpr.push_back("StoreInst " + Name + " is atomic");


  if ( ConstantInt *ConstData = dyn_cast<ConstantInt>(DataVal) ) {
    std::stringstream NameStream;
    NameStream << ConstData->getSExtValue();

  } else if ( ConstantFP *ConstData = dyn_cast<ConstantFP>(DataVal) ) {
    APFloat F = ConstData->getValueAPF();
  } else {
  }

  //Get Address to store at
  
  //Address is either indexed or direct
  
  if ( GetElementPtrInst *TargetInst = dyn_cast<GetElementPtrInst>(AddrVal) ) {

    Value *TargetBase       = TargetInst->getPointerOperand();

  } else {
  }
}
void OCLAccSPIRCheckVisitor::visitGetElementPtrInst(GetElementPtrInst &I) {
  Value *CallVal = &I;

  if ( ! I.isInBounds() )
    llvm_unreachable("Not in Bounds.");

  Type *BaseType = I.getPointerOperandType();
  if ( ! BaseType )
    llvm_unreachable( "Type expected." );

  Value *BaseVal = I.getPointerOperand();
  if ( ! BaseVal )
    llvm_unreachable( "Value expected." );

  std::string Name = I.getName().str();

  //Get HW for base

  unsigned BaseAddressSpace = I.getPointerAddressSpace();
  if ( BaseAddressSpace != ocl::AS_GLOBAL )
    llvm_unreachable( "NOT_IMPLEMENTED: Only global address space supported." );

  if ( I.getNumIndices() != 1 )
    llvm_unreachable("Only 1D arrays supported");

  Value *IndexVal = *(I.idx_begin());

  if ( I.hasAllConstantIndices() ) {

    ConstantInt *IndexConst = dyn_cast<ConstantInt>(IndexVal);
    if ( ! IndexConst  )
      llvm_unreachable( "Constant Index not Integer" );

    errs() << BaseVal->getName() << "[" << IndexConst->getValue() << "]\n";
  }
}

/* CoOCLAccSPIRCheckVisitor::nversion Operations */
void OCLAccSPIRCheckVisitor::visitTruncInst(TruncInst &I) {}
void OCLAccSPIRCheckVisitor::visitZExtInst(ZExtInst &I) {}
void OCLAccSPIRCheckVisitor::visitSExtInst(SExtInst &I) {}
void OCLAccSPIRCheckVisitor::visitFPTruncInst(FPTruncInst &I) {}
void OCLAccSPIRCheckVisitor::visitFPExtInst(FPExtInst &I) {}
void OCLAccSPIRCheckVisitor::visitFPToUIInst(FPToUIInst &I) {}
void OCLAccSPIRCheckVisitor::visitFPToSIInst(FPToSIInst &I) {}
void OCLAccSPIRCheckVisitor::visitUIToFPInst(UIToFPInst &I) {}
void OCLAccSPIRCheckVisitor::visitSIToFPInst(SIToFPInst &I) {}
void OCLAccSPIRCheckVisitor::visitPtrToIntInst(PtrToIntInst &I) {}
void OCLAccSPIRCheckVisitor::visitIntToPtrInst(IntToPtrInst &I) {}
void OCLAccSPIRCheckVisitor::visitBitCastInst(BitCastInst &I) {}
void OCLAccSPIRCheckVisitor::visitAddrSpaceCastInst(AddrSpaceCastInst &I) {}

/* OtOCLAccSPIRCheckVisitor::her Operations */
void OCLAccSPIRCheckVisitor::visitICmpInst(ICmpInst &I) {}
void OCLAccSPIRCheckVisitor::visitFCmpInst(FCmpInst &I) {}
void OCLAccSPIRCheckVisitor::visitPHINode(PHINode &I) {}
void OCLAccSPIRCheckVisitor::visitSelectInst(SelectInst &I) {}
void OCLAccSPIRCheckVisitor::visitCallInst (CallInst &I) {
  Value *CallVal = &I;
  const Value *Callee = I.getCalledValue();
  std::string CalleeName = Callee->getName().str();

  /* Work-item Functions */
  if ( CalleeName == "get_global_id" ) {
  } else if ( CalleeName == "get_work_dim" ) {
  } else if ( CalleeName == "get_global_size" ) {
  } else if ( CalleeName == "get_global_id" ) {
  } else if ( CalleeName == "get_local_size" ) {
  } else if ( CalleeName == "get_enqueued_local_size" ) {
  } else if ( CalleeName == "get_local_id" ) {
  } else if ( CalleeName == "get_num_groups" ) {
  } else if ( CalleeName == "get_group_id" ) {
  } else if ( CalleeName == "get_global_offset" ) {
  } else if ( CalleeName == "get_global_linear_id" ) {
  } else if ( CalleeName == "get_local_linear_id" ) {
  }

  /* Math Functions */
  else if ( CalleeName == "acos" ) {
  } else if ( CalleeName == "acosh" ) {
  }

  /* Integer Functions */
  else if ( CalleeName == "abs" ) {
  } else if ( CalleeName == "abs_diff" ) {
  }

  /* Common Functions */
  /* Geometric Functions */
  /* Relational Functions */
  /* Vector Load and Store Functions */

  /* Synchronization Functions */
  else if ( CalleeName == "work_group_barrier" ) {
  } 
  
  /* Address Space Qualifier Functions */
  else if ( CalleeName == "to_global" ) {
  } else if ( CalleeName == "to_local" ) {
  } else if ( CalleeName == "to_private" ) {
  } else if ( CalleeName == "get_fence" ) {
  } 

  /* Async Copies from Global->Local, Local->Global, Prefetch */
  /* Atomic Functions */
  /* Miscellaneous Vector Functions */

  /* printf */
  else if ( CalleeName == "printf" ) {
  } 
  
  /* LLVM Functions */
  else if ( CalleeName == "llvm.dbg.value" ) {
    //nothing 
  }

  else {
    //errs() << "Function " << CalleeName << "\n";
    //llvm_unreachable( "Only OpenCL-Builtin Functions supported." );
  }

}

void OCLAccSPIRCheckVisitor::visitArgument(Argument &I) {
  Type * ArgType = I.getType();
  Value *Val = &I;
  std::string Name = I.getName().str();

  if ( ArgType->isPointerTy() ) {

    bool isRead = false;
    bool isWritten = false;

    if ( I.onlyReadsMemory() ) {
      isRead    = true;
    } else {

      // have to check the whole hierarchy

      std::list<Value *> Values;
      Values.push_back(&I);

      while ( ! Values.empty() ) {
        Value *CurrVal = Values.back();
        Values.pop_back();

        StoreInst *St = dyn_cast<StoreInst>(CurrVal);
        if ( St ) {
          isWritten = true;
          continue;
        } 

        LoadInst *Ld = dyn_cast<LoadInst>(CurrVal);
        if ( Ld ) {
          isRead = true;
          continue;
        }

        for ( auto &Inst : CurrVal->uses() ) {
          Value *V = Inst.getUser();
       //   errs() << "Append " << V->getName() << " for " << CurrVal->getName() << ". Size: " << Values.size() << "\n";
          Values.push_back(V);
        }
      }
    }

    Type *ElementType   = ArgType->getPointerElementType();
    unsigned SizeInBits = ElementType->getScalarSizeInBits();

    unsigned AddressSpace = ArgType->getPointerAddressSpace();

    switch ( AddressSpace ) {
      case ocl::AS_GLOBAL:
        break;
      case ocl::AS_LOCAL:
        break;
      default:
        llvm_unreachable( "AddressSpace not supported" );
    }
  } else if ( ArgType->isIntegerTy() ) {

  } else if ( ArgType->isFloatingPointTy() ) {

  } else {
    llvm_unreachable("Unknown Argument Type");
  }
}

void OCLAccSPIRCheckVisitor::visitCmpInst(CmpInst &I)
{
  Value *Cmp = &I;
  std::string Name = I.getName().str();
  //errs() << "CmpInst " << Name << "\n";

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
      break;
    case CmpInst::ICMP_NE:
      //errs() << "Integer NEQ\n";
      break;
    case CmpInst::ICMP_SGT:
      //errs() << "Signed GT\n";
      break;
    case CmpInst::ICMP_SGE:
      //errs() << "Signed GE\n";
    case CmpInst::ICMP_SLT:
      //errs() << "Signed LT\n";
      break;
    case CmpInst::ICMP_SLE:
      //errs() << "Signed LE\n";
      break;
    default:
      errs() << "Compare Predicate: " << Pred << "\n";
      llvm_unreachable("Unsupported Compare-Type");
  }

  for (auto &Op : I.operands() ) {
    Value *OpVal = Op.get();

    if ( const ConstantInt *IntArg = dyn_cast<ConstantInt>(OpVal) ) {
      int64_t IntVal = IntArg->getSExtValue();
    } else {
    }
  }
}

} //end namespace llvm

