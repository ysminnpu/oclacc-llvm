#include "HW/Arith.h"
#include "OCLAccHW.h"

#define DEBUG_TYPE "gep"

using namespace llvm;
using namespace oclacc;

base_p OCLAccHW::computeSequentialIndex(BasicBlock *Parent, Value *IV, SequentialType *IndexTy) {
  block_p HWParent = getBlock(Parent);

  const std::string Name = IV->getName();

  base_p HWOffset = nullptr;

  Type *ThisTy = IndexTy->getElementType();

  if (ConstantInt *C = dyn_cast<ConstantInt>(IV)) {
    // If the current index is zero, just skip it
    if (C->isZero()) {
      // do nothing
      ODEBUG("    Size: " << DL->getTypeAllocSize(ThisTy));
      ODEBUG("    Index: 0");

      return nullptr;
    }
    // get constant value and multiply by size of current element
    APInt A = APInt(64, C->getZExtValue(), false);
    uint64_t CurrSize = DL->getTypeAllocSize(ThisTy);

    ODEBUG("    Size: " << CurrSize);
    ODEBUG("    Index: " << A.toString(10, false));

    A *= APInt(64, CurrSize, false);

    HWOffset = std::make_shared<ConstVal>(
        A.toString(10, false),
        Datatype::Integer,
        A.toString(2, false),
        A.getActiveBits());
  } else {
    // For the index, there must be an HW instance available.
    base_p HWLocalIndex = getHW(Parent, IV);

    // Get the size of the Type
    uint64_t CurrSize = DL->getTypeAllocSize(ThisTy);

    ODEBUG("    Size: " << CurrSize);
    ODEBUG("    Index: " << IV->getName());

    // Insert shifter if size is a power of two or a multiplication otherwise.
    APInt CurrSizeAP(64, CurrSize, false);
    uint64_t AddrWidth = CurrSizeAP.getActiveBits();


    int32_t Log = CurrSizeAP.exactLogBase2();

    if (Log != -1) {
      APInt LogAP(32, Log, false);
      const_p HWLogSize = std::make_shared<ConstVal>(std::to_string(Log), LogAP.toString(2, false), LogAP.getActiveBits());
//HWLogSize->setParent(HWParent);

      // Power of two, use left shift
      AddrWidth = Log + HWLocalIndex->getBitWidth();
      std::string IndexName = Name+"_shift";

      shl_p HWShift = std::make_shared<Shl>(IndexName, AddrWidth);
// HWShift->setParent(HWParent);
      HWParent->addOp(HWShift);

      HWParent->addConstVal(HWLogSize);

      connect(HWLocalIndex, HWShift);
      connect(HWLogSize,HWShift);

      HWOffset = HWShift;
    } else {
      const_p HWSize = std::make_shared<ConstVal>(std::to_string(CurrSize), CurrSizeAP.toString(2, false), AddrWidth);
//      HWSize->setParent(HWParent);
      // No power of two, use multiplication
      AddrWidth = CurrSizeAP.getActiveBits() + HWLocalIndex->getBitWidth();
      std::string IndexName = Name+"_mul";

      mul_p HWMul = std::make_shared<Mul>(IndexName, AddrWidth);
//      HWMul->setParent(HWParent);
      HWParent->addOp(HWMul);

      HWParent->addConstVal(HWSize);

      connect(HWLocalIndex, HWMul);
      connect(HWSize, HWMul);

      HWOffset = HWMul;
    }
  }
  return HWOffset;
}

const_p OCLAccHW::computeStructIndex(BasicBlock *Parent, Value *IV, StructType *IndexTy) {
  block_p HWParent = getBlock(Parent);

  const std::string Name = IV->getName();

  const_p HWOffset = nullptr;

  ConstantInt *C = dyn_cast<ConstantInt>(IV);
  assert(C && "Struct Index must be constant.");

  uint64_t Index = C->getZExtValue();

  // If the current index is zero, just skip it
  if (C->isZero()) {
    // do nothing
    ODEBUG("    Struct Offset: " << DL->getTypeAllocSize(IndexTy));
    ODEBUG("    Index: 0");

    return nullptr;
  } 

  // get constant value and multiply by size of current element
  APInt A = APInt(64, Index, false);
  uint64_t Offset = 0;
  
  // Skip all Struct elements before the actual one
  for (uint64_t i = 0; i < Index; ++i) {
    Offset += DL->getTypeAllocSize(IndexTy->getTypeAtIndex(i));
  }

  ODEBUG("  Struct Offset: " << Offset);
  ODEBUG("  Index: " << A.toString(10, false));

  A *= APInt(64, Offset, false);

  HWOffset = std::make_shared<ConstVal>(
      A.toString(10, false),
      Datatype::Integer,
      A.toString(2, false),
      A.getActiveBits());

  HWParent->addConstVal(HWOffset);

  return HWOffset;
}

void OCLAccHW::handleGEPOperator(GEPOperator &I) {
  BasicBlock *Parent;

  // GEP can an Instruction or used as operator by load or store
  if (Instruction *II = dyn_cast<Instruction>(&I))
    Parent = II->getParent();
  else if (I.hasOneUse()) {
    Instruction *II = cast<Instruction>(I.use_begin()->getUser());
    Parent = II->getParent();
  } else
    assert(0 && "No Parent");

  block_p HWParent = getBlock(Parent);

  Function *F= Parent->getParent();
  kernel_p HWF = getKernel(F);

  Value *InstValue = &I;
  Value *BaseValue = I.getPointerOperand();

  SequentialType *IType = cast<SequentialType>(I.getType());

  if (! I.isInBounds() )
    assert(0 && "Not in Bounds.");

  std::string Name = I.getName();
  if (!Name.size())
    Name = "unnamed_idx";

  // The pointer base can either be a local Array or a input stream
  unsigned BaseAddressSpace = I.getPointerAddressSpace();
  assert((BaseAddressSpace == ocl::AS_GLOBAL || BaseAddressSpace == ocl::AS_LOCAL)
      && "Only global and local address space supported." );

  streamport_p HWStream = getHW<StreamPort>(Parent, BaseValue);

  // If it is a local array, it may not have been added to the kernel
  HWF->addStream(HWStream);

  ODEBUG("GEP " << Name);

  // Walk throught the indices. The GEP instruction may have multiple indices
  // when arrays or structures are accessed.

  // Constant zero address
  if (I.hasAllZeroIndices()) {

    const_p HWIndex = std::make_shared<ConstVal>("0", "0", 1);
    HWParent->addConstVal(HWIndex);

    streamindex_p HWStreamIndex = makeHWBB<StaticStreamIndex>(Parent, InstValue, Name, HWStream, HWIndex, 1);
    connect(HWIndex, HWStreamIndex);

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
  CompositeType *NextTy = BasePointer;

  // We can compute the Index at compile time. Walk through all indices and
  // types and compute the Index in an APInt. Finally save its numerical value
  // as StaticStreamIndex.

  if (I.hasAllConstantIndices()) {
    uint64_t Offset = 0;

    for (User::op_iterator II = I.idx_begin(), E = I.idx_end(); II != E; ++II) {
      Value *IV = *II;

      assert(NextTy);

      // All are Constants
      ConstantInt *C = cast<ConstantInt>(*II);
      uint64_t Index = C->getZExtValue();

      if (SequentialType *ST = dyn_cast<SequentialType>(NextTy)) {
        // In bytes, incl padding
        Type *ThisTy = ST->getElementType();
        uint64_t CurrSize = DL->getTypeAllocSize(ThisTy);

        ODEBUG("    Size: " << CurrSize);
        ODEBUG("    Index: " << Index);

        Offset += Index * CurrSize;

      } else if (StructType *ST = dyn_cast<StructType>(NextTy)) {
        // Compute offset for requested index
        uint64_t LOffset = 0;
        for (uint64_t i = 0; i < Index; ++i) {
          LOffset += DL->getTypeAllocSize(ST->getTypeAtIndex(i));
        }

        ODEBUG("  Struct Offset: " << LOffset);
        ODEBUG("  Index: " << Index);

        Offset += LOffset;
      } else
        assert(0 && "Invalid Type");

      NextTy = dyn_cast<CompositeType>(NextTy->getTypeAtIndex(IV));
    }

    APInt APOffset(64, Offset, false);

    uint64_t BitWidth = APOffset.getActiveBits();

    const_p HWIndex = std::make_shared<ConstVal>(APOffset.toString(10,false), APOffset.toString(2, false), BitWidth);
    HWParent->addConstVal(HWIndex);

    streamindex_p HWStreamIndex = makeHWBB<StaticStreamIndex>(Parent, InstValue, Name, HWStream, HWIndex, BitWidth);
    connect(HWIndex, HWStreamIndex);

    BlockValueMap[Parent][InstValue] = HWStreamIndex;

    ODEBUG("  Static index " << HWStreamIndex->getUniqueName() << " = " << Offset);

    return;
  }

  // Mixed indices, so we have to arithmetically compute the address at runtime.
  // Use shifter for the multiplication with the scalar sizes. Indizes can refer
  // to SequentialTypes or StructTypes. The latter must have constant indices.
  
  Value *CurrValue;

  base_p HWIndex = nullptr;

  int IndexNo = 0;
  for (User::op_iterator II = I.idx_begin(), E = I.idx_end(); II != E; ++II, IndexNo++) {
    Value *IV = *II;

    assert(NextTy);

    base_p HWOffset = nullptr;

    // Correctnes of cast will be checked within the next Index operand
    // Set NextTy after getting the offset
    if (SequentialType *ST = dyn_cast<SequentialType>(NextTy)) {
      HWOffset = computeSequentialIndex(Parent, IV, ST);
    }
    else if (StructType *ST = dyn_cast<StructType>(NextTy)) {
      // Set NextTy as Index is retrieved by the function.
      HWOffset = computeStructIndex(Parent, IV, ST);
    } else
      assert(0 && "Invalid Type");

    DEBUG(
        dbgs() << "[" << DEBUG_TYPE << "] " << "  Index " << IndexNo << ": ";
        NextTy->print(dbgs());
        dbgs() << "\n";
        );

    // We have no offset if the index was a constant zero
    if (HWOffset) {
      // If we already have an address computation, add the current index
      if (HWIndex) {
        // The Address may grow by one bit
        uint64_t AddrWidth = std::max(HWOffset->getBitWidth(), HWIndex->getBitWidth())+1;
        std::string IndexName = Name+"_"+std::to_string(IndexNo)+"_add";

        base_p HWAdd = std::make_shared<Add>(IndexName, AddrWidth);
        HWAdd->setParent(HWParent);
        HWParent->addOp(HWAdd);

        connect(HWOffset, HWAdd);
        connect(HWIndex, HWAdd);

        HWIndex = HWAdd;
      } else
        HWIndex = HWOffset;
    }

    NextTy = dyn_cast<CompositeType>(NextTy->getTypeAtIndex(IV));
  }

  streamindex_p HWStreamIndex = makeHWBB<DynamicStreamIndex>(Parent, InstValue, Name, HWStream, HWIndex, HWIndex->getBitWidth());

  connect(HWIndex, HWStreamIndex);

  BlockValueMap[Parent][InstValue] = HWStreamIndex;
}
