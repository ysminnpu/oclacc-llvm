//===- BitWidthAnalysis.cpp -----------------------------------------------===//
//
// TODO: Add SCEV support in forward propagation.
//
//===----------------------------------------------------------------------===//

#define DEBUG_TYPE "bitwidthanalysis"

#include "BitWidthAnalysis.h"

#include "llvm/ADT/APFloat.h"
#include "llvm/ADT/APInt.h"
#include "llvm/ADT/PostOrderIterator.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/IR/CFG.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/Support/Debug.h"

#include <set>
#include <list>

using namespace llvm;

STATISTIC(StatsNumIterations, "Number of iterations");

/// \brief Returns the bitwidth that is at least needed for the given constant.
///
/// Computes the bitwidth at least needed for the given constant. If the instruction
/// that uses it is given first a lookup in the internal map is performed: if
/// a bitwidth could be found there, it is returned. Else the bitwidth is computed
/// and (if possible) stored in the map.
int BitWidthAnalysis::getBitWidthConstant(const Constant *C, bool isSigned,
    const Instruction *OwningI) {
  if (C == 0) { return -1; }

  if (OwningI != 0) {
    // The owning instruction is given, so search the map for it
    ConstantBitWidthMapTy::iterator CPos = ConstantBWMap.find(std::make_pair(C, OwningI));
    if (CPos != ConstantBWMap.end()) {
      if (CPos->second.Valid == true) {
        return CPos->second.OutBitwidth;
      }
    }
  }

  struct BitWidth CBW;
  CBW.TypeWidth = DL->getTypeSizeInBits(C->getType());
  CBW.OutBitwidth = DL->getTypeSizeInBits(C->getType());
  CBW.MaxBitwidth = DL->getTypeSizeInBits(C->getType());

  // The lookup table does not yet contain an entry for that const or no owning
  // instruction is given
  if (isa<ConstantInt>(C) == true) {
    const ConstantInt *CInt = dyn_cast<ConstantInt>(C);
    if (isSigned == true) {
      // The int should be assumed to be signed
      CBW.OutBitwidth = CInt->getValue().getMinSignedBits();
      CBW.ValueMaskBitwidth = CInt->getValue().getMinSignedBits();
      CBW.Ext = Loopus::ExtKind::SExt;
      CBW.Valid = true;
    } else {
      // The int should be assumed to be unsigned
      CBW.OutBitwidth = CInt->getValue().getActiveBits();
      CBW.ValueMaskBitwidth = CInt->getValue().getActiveBits();
      CBW.Ext = Loopus::ExtKind::ZExt;
      CBW.Valid = true;
    }
  } else if (isa<ConstantFP>(C) == true) {
    CBW.ValueMaskBitwidth = -1;
    CBW.Ext = Loopus::ExtKind::FPNoExt;
    CBW.Valid = true;
  }

  if (CBW.Valid == true) {
    // Insert bitwidth information into the map for later use again
    if (OwningI != 0) {
      const auto CBWMKey = std::make_pair(C, OwningI);
      ConstantBWMap[CBWMKey] = CBW;
    }
    return CBW.OutBitwidth;
  } else {
    return -1;
  }
}

std::pair<int, Loopus::ExtKind> BitWidthAnalysis::getBitWidthRaw(const Value *V,
    bool isSigned, const Instruction *OwningI) {
  const std::pair<int, Loopus::ExtKind> UndefRetVal =
      std::make_pair(-1, Loopus::ExtKind::Undef);
  if (V == 0) { return UndefRetVal; }

  BitWidthMapTy::iterator VIT = BWMap.find(V);
  if (VIT == BWMap.end()) {
    // The bitwidth information for the requested are not available
    if (isa<Constant>(V) == true) {
      // The requested value is a constant and therefore not stored in the map
      // so compute it on the fly
      const Constant *C = dyn_cast<Constant>(V);
      getBitWidthConstant(C, isSigned, OwningI);

      ConstantBitWidthMapTy::iterator OpIT = ConstantBWMap.find(std::make_pair(C, OwningI));
      if (OpIT != ConstantBWMap.end()) {
        return std::make_pair(OpIT->second.OutBitwidth, OpIT->second.Ext);
      } else {
        const int ITypeWidth = DL->getTypeSizeInBits(C->getType());
        return std::make_pair(ITypeWidth, Loopus::ExtKind::SExt);
      }
    } else if (isa<Instruction>(V) == true) {
      // The requested value is an instruction that was not inspected yet. So
      // just use the result type as a first estimation.
      const int ITypeWidth = DL->getTypeSizeInBits(V->getType());
      return std::make_pair(ITypeWidth, Loopus::ExtKind::SExt);
    } else if (isa<Argument>(V) == true) {
      // The requested value is an argument. As there are no further information
      // available use the type bitwidth.
      const int ITypeWidth = DL->getTypeSizeInBits(V->getType());
      return std::make_pair(ITypeWidth, Loopus::ExtKind::SExt);
    } else {
      // This is the case for Metadata, BasicBlocks,...
      return UndefRetVal;
    }
  } else {
    // We have already computed the bitwidth information for the requested value
    if (VIT->second.Valid == true) {
      return std::make_pair(VIT->second.OutBitwidth, VIT->second.Ext);
    } else {
      return UndefRetVal;
    }
  }
}

int BitWidthAnalysis::getBitWidth(const Value *V, bool isSigned,
    const Instruction *OwningI) {
  std::pair<int, Loopus::ExtKind> VBW = getBitWidthRaw(V, isSigned, OwningI);
  if ((VBW.first > 0) && (VBW.second != Loopus::ExtKind::Undef)) {
    if (isSigned == true) {
      return VBW.first;
    } else {
      const int ITypeWidth = DL->getTypeSizeInBits(V->getType());
      if (VBW.second == Loopus::ExtKind::SExt) {
        // The requested value performs a sign extension but the unsigned value
        // is required. So the signal has to be as wide as the datatype as it
        // might filled up with the sign value
        return ITypeWidth;
      } else if (VBW.second == Loopus::ExtKind::ZExt) {
        return VBW.first;
      } else if (VBW.second == Loopus::ExtKind::OneExt) {
        return ITypeWidth;
      } else if (VBW.second == Loopus::ExtKind::FPNoExt) {
        return VBW.first;
      } else {
        return -1;
      }
    }
  } else {
    return -1;
  }
}

/// \brief Determines the bitwidth of the largest operand of the given instruction.
int BitWidthAnalysis::getBitWidthLargestOp(const Instruction *I, bool isSigned) {
  if (I == 0) { return -1; }
  if (I->getNumOperands() == 0) {
    return -1;
  } else if (I->getNumOperands() == 1) {
    return getBitWidth(I->getOperand(0), isSigned, I);
  } else {
    int maxBWOp = -1;
    for (unsigned i = 0, e = I->getNumOperands(); i < e; ++i) {
      int BWCurOp = getBitWidth(I->getOperand(i), isSigned, I);
      if (maxBWOp > 0) {
        if ((BWCurOp > 0) && (BWCurOp > maxBWOp)) {
          maxBWOp = BWCurOp;
        }
      } else {
        maxBWOp = BWCurOp;
      }
    }
    return maxBWOp;
  }
}

//===- Implementation of forward transition functions ---------------------===//
//===----------------------------------------------------------------------===//

bool BitWidthAnalysis::forwardUpdateWidth(struct BitWidth &BWInfo,
    int newOutWidth, int newValueMaskWidth, int newMaxWidth) {
  bool changed = false;

  BWInfo.MaxBitwidth = newMaxWidth;

  // Update the value mask width
  if (newValueMaskWidth != BWInfo.ValueMaskBitwidth) {
    BWInfo.ValueMaskBitwidth = newValueMaskWidth;
    changed = true;
  }

  // Update the new out bitwidth and use the smallest width possible
  if ((BWInfo.TypeWidth > 0) && (newOutWidth > BWInfo.TypeWidth)) {
    // Should not exceed the type width
    newOutWidth = BWInfo.TypeWidth;
  }
  if ((BWInfo.ValueMaskBitwidth > 0) && (newOutWidth > BWInfo.ValueMaskBitwidth)) {
    // Based on the value range by SCEV the bitwidth can be reduced
    newOutWidth = BWInfo.ValueMaskBitwidth;
  }
  if ((BWInfo.RequiredBitwidth > 0) && (newOutWidth > BWInfo.RequiredBitwidth)) {
    // Reduce the width to the width required by all users if possible
    newOutWidth = BWInfo.RequiredBitwidth;
  }

  if (newOutWidth != BWInfo.OutBitwidth) {
    BWInfo.OutBitwidth = newOutWidth;
    changed = true;
  }

  // If the bitwidth info are not valid then we cannot change anything (although
  // we might have changed any values)
  if (BWInfo.Valid == false) {
    changed = false;
  }

  return changed;
}

bool BitWidthAnalysis::forwardUpdateOrInsertWidth(const Instruction *I,
    int newOutWidth, int newValueMaskWidth, int newMaxWidth, int newTypeWidth,
    Loopus::ExtKind newExt) {
  if (I == 0) { return false; }

  // Check if we already have an entry for this instruction
  BitWidthMapTy::iterator IIT = BWMap.find(I);
  if (IIT == BWMap.end()) {
    // No entry available. Create a new one and initialize it
    struct BitWidth CBW;
    CBW.TypeWidth = newTypeWidth;
    CBW.Ext = newExt;

    // Update struct info
    forwardUpdateWidth(CBW, newOutWidth, newValueMaskWidth, newMaxWidth);
    CBW.Valid = true;
    BWMap[I] = CBW;

    // We have changed something...
    return true;
  } else {
    // There is already an entry so update if needed
    bool changed = forwardUpdateWidth(IIT->second, newOutWidth, newValueMaskWidth, newMaxWidth);
    return changed;
  }
}

bool BitWidthAnalysis::forwardSetWidth(struct BitWidth &BWInfo,
    int newOutWidth, int newValueMaskWidth, int newMaxWidth) {
  bool changed = false;

  BWInfo.MaxBitwidth = newMaxWidth;

  // Update the value mask width
  if (newValueMaskWidth != BWInfo.ValueMaskBitwidth) {
    BWInfo.ValueMaskBitwidth = newValueMaskWidth;
    changed = true;
  }

  // Again: avoid to make the output wider than the declared type
  if ((BWInfo.TypeWidth > 0) && (newOutWidth > BWInfo.TypeWidth)) {
    newOutWidth = BWInfo.TypeWidth;
  }
  if (newOutWidth != BWInfo.OutBitwidth) {
    BWInfo.OutBitwidth = newOutWidth;
    changed = true;
  }

  // If the bitwidth info are not valid then we cannot change anything (although
  // we might have changed any values)
  if (BWInfo.Valid == false) {
    changed = false;
  }

  return changed;
}

bool BitWidthAnalysis::forwardSetOrInsertWidth(const Instruction *I,
    int newOutWidth, int newValueMaskWidth, int newMaxWidth, int newTypeWidth,
    Loopus::ExtKind newExt) {
  if (I == 0) { return false; }

  // Check if we already have an entry for this instruction
  BitWidthMapTy::iterator IIT = BWMap.find(I);
  if (IIT == BWMap.end()) {
    // No entry available. Create a new one and initialize it
    struct BitWidth CBW;
    CBW.TypeWidth = newTypeWidth;
    CBW.Ext = newExt;

    // Update struct info
    forwardSetWidth(CBW, newOutWidth, newValueMaskWidth, newMaxWidth);
    CBW.Valid = true;
    BWMap[I] = CBW;

    // We have changed something...
    return true;
  } else {
    // There is already an entry so update if needed
    bool changed = forwardSetWidth(IIT->second, newOutWidth, newValueMaskWidth, newMaxWidth);
    return changed;
  }
}

//===- Arithmetic instructions --------------------------------------------===//
// For arithmetic integer instructions each input operand has to be extended
// according to its ExtKind-flag (sext, zext) to the width of the widest
// input operand.
// In most cases the result will carry the SExt-flag except those instructions
// that are explicitly unsigned operations (e.g. udiv, urem,...), which will
// have the ZExt-flag.
// Pointer types can be ignored as - according LLVM IR specification - they are
// not allowed as input operand for arithmetic instructions.

bool BitWidthAnalysis::forwardHandleAddSub(const BinaryOperator *AI) {
  if (AI == 0) { return false; }

  // Determine size of larger operand
  const int maxBW = getBitWidthLargestOp(AI, true);

  const int ITypeWidth = DL->getTypeSizeInBits(AI->getType());
  bool changed = forwardUpdateOrInsertWidth(AI, maxBW+1, -1, maxBW+1,
      ITypeWidth, Loopus::ExtKind::SExt);
  printDBG(AI);
  return changed;
}

bool BitWidthAnalysis::forwardHandleMul(const BinaryOperator *MI) {
  if (MI == 0) { return false; }

  // Determine sum of bitwidth of all operands
  int sumBWOps = 0;
  for (unsigned i = 0, e = MI->getNumOperands(); i < e; ++i) {
    int tmpBW = getBitWidth(MI->getOperand(i), true, MI);
    if (tmpBW > 0) {
      sumBWOps = sumBWOps + tmpBW;
    }
  }

  const int ITypeWidth = DL->getTypeSizeInBits(MI->getType());
  bool changed = forwardUpdateOrInsertWidth(MI, sumBWOps, -1, sumBWOps,
      ITypeWidth, Loopus::ExtKind::SExt);
  printDBG(MI);
  return changed;
}

bool BitWidthAnalysis::forwardHandleSDiv(const BinaryOperator *SDI) {
  // Handle division of SIGNED values
  if (SDI == 0) { return false; }

  const int ITypeWidth = DL->getTypeSizeInBits(SDI->getType());
  // The result can at most be as wide as the first operand
  int firstOpBW = getBitWidth(SDI->getOperand(0), true, SDI);
  if (firstOpBW <= 0) {
    firstOpBW = ITypeWidth;
  }

  bool changed = forwardUpdateOrInsertWidth(SDI, firstOpBW, -1, firstOpBW,
      ITypeWidth, Loopus::ExtKind::SExt);
  printDBG(SDI);
  return changed;
}

bool BitWidthAnalysis::forwardHandleUDiv(const BinaryOperator *UDI) {
  // Handle division of UNSIGNED values
  if (UDI == 0) { return false; }

  const int ITypeWidth = DL->getTypeSizeInBits(UDI->getType());
  // The result can at most be as wide as the first operand
  int firstOpBW = getBitWidth(UDI->getOperand(0), false, UDI);
  if (firstOpBW <= 0) {
    firstOpBW = ITypeWidth;
  }

  bool changed = forwardUpdateOrInsertWidth(UDI, firstOpBW, -1, firstOpBW,
      ITypeWidth, Loopus::ExtKind::ZExt);
  printDBG(UDI);
  return changed;
}

bool BitWidthAnalysis::forwardHandleSRem(const BinaryOperator *SRI) {
  if (SRI == 0) { return false; }

  const int ITypeWidth = DL->getTypeSizeInBits(SRI->getType());
  // The result can at most be as wide as the first operand
  int secondOpBW = getBitWidth(SRI->getOperand(1), true, SRI);
  if (secondOpBW <= 0) {
    secondOpBW = ITypeWidth;
  }

  bool changed = forwardUpdateOrInsertWidth(SRI, secondOpBW, -1, secondOpBW,
      ITypeWidth, Loopus::ExtKind::SExt);
  printDBG(SRI);
  return changed;
}

bool BitWidthAnalysis::forwardHandleURem(const BinaryOperator *URI) {
  if (URI == 0) { return false; }

  const int ITypeWidth = DL->getTypeSizeInBits(URI->getType());
  // The result can at most be as wide as the first operand
  int secondOpBW = getBitWidth(URI->getOperand(1), false, URI);
  if (secondOpBW <= 0) {
    secondOpBW = ITypeWidth;
  }

  bool changed = forwardUpdateOrInsertWidth(URI, secondOpBW, -1, secondOpBW,
      ITypeWidth, Loopus::ExtKind::ZExt);
  printDBG(URI);
  return changed;
}

//===- Bitwise logical instructions ---------------------------------------===//
// Depending on the performed instruction the input operands might have to be
// extended according to their own ExtKind-flag.
// The result signal of the bitwise logical instructions will always have the
// zext-flag set.

bool BitWidthAnalysis::forwardHandleAnd(const BinaryOperator *AI) {
  if (AI == 0) { return false; }

  const int ITypeWidth = DL->getTypeSizeInBits(AI->getType());
  // This is not as easy as expected: if there is at least one zext'ed input
  // signal, the output is as wide as the smallest zext'ed input.
  // If all inputs are sext'ed then the output is as wide as the output
  // datatype width as the sign-bits cannot be determined and are unknown. So
  // it might be that all sign-bits are 1 and therefor the result is also one-
  // extended. On the other hand we cannot use the sext-flag as there could be
  // an input with a 0 as sign-bit and then the output is zero from that bit on
  // (but the sign-bits are unknown).
  // The getBitWidth function now takes care about that and handles bitwidth
  // requests properly depending on the signedness of the signal and the given
  // argument.
  int minZExtBW = -1;
  for (unsigned i = 0, e = AI->getNumOperands(); i < e; ++i) {
    const Value *CurOP = AI->getOperand(i);
    const int curOpBW = getBitWidth(CurOP, false, AI);
    if (minZExtBW > 0) {
      if ((curOpBW > 0) && (curOpBW < minZExtBW)) {
        minZExtBW = curOpBW;
      }
    } else {
      minZExtBW = curOpBW;
    }
  }
  if (minZExtBW <= 0) {
    minZExtBW = ITypeWidth;
  }

  bool changed = forwardUpdateOrInsertWidth(AI, minZExtBW, minZExtBW, minZExtBW,
      ITypeWidth, Loopus::ExtKind::ZExt);
  printDBG(AI);
  return changed;
}

bool BitWidthAnalysis::forwardHandleOr(const BinaryOperator *OI) {
  if (OI == 0) { return false; }

  const int ITypeWidth = DL->getTypeSizeInBits(OI->getType());
  // Not that easy: if all input operands are zext'ed, the output signal is as
  // wide as the largest input signal and then zext'ed.
  // If at least one input signal is not zext'ed, the output signal will as
  // wide as the output datatype width (same reasons as for the and-handler).
  int maxZExtBW = -1;
  for (unsigned i = 0, e = OI->getNumOperands(); i < e; ++i) {
    const Value *curOP = OI->getOperand(i);
    const int curOpBW = getBitWidth(curOP, false, OI);
    if (maxZExtBW > 0) {
      if ((curOpBW > 0) && (curOpBW > maxZExtBW)) {
        maxZExtBW = curOpBW;
      }
    } else {
      maxZExtBW = curOpBW;
    }
  }
  if (maxZExtBW <= 0) {
    maxZExtBW = ITypeWidth;
  }

  // The result can at most be as wide as the largest operand
  bool changed = forwardUpdateOrInsertWidth(OI, maxZExtBW, maxZExtBW, maxZExtBW,
      ITypeWidth, Loopus::ExtKind::ZExt);
  printDBG(OI);
  return changed;
}

bool BitWidthAnalysis::forwardHandleShl(const BinaryOperator *SI) {
  if (SI == 0) { return false; }

  bool changed = false;
  const int ITypeWidth = DL->getTypeSizeInBits(SI->getType());

  // Collect input operand information
  const Value *Op = SI->getOperand(0);
  BitWidthMapTy::iterator OpIT = BWMap.find(Op);
  if ((OpIT != BWMap.end()) && (OpIT->second.Valid == true)) {
    const Value *SOp = SI->getOperand(1);
    if (isa<ConstantInt>(SOp) == true) {
      const struct BitWidth &IOBW = OpIT->second;
      // Determine the shift length
      const ConstantInt *SLOp = dyn_cast<ConstantInt>(SOp);
      int ShiftLength = SLOp->getZExtValue();
      if (ShiftLength > ITypeWidth) {
        errs() << "WARNING: " << *SI << ": value shifted left more bits than "
            << "allowed by type width results in undefined behaviour!\n";
      }
      int outBW = ITypeWidth;
      if ((IOBW.Valid == true) && (IOBW.OutBitwidth > 0)) {
        outBW = IOBW.OutBitwidth + ShiftLength;
      }
      changed = forwardUpdateOrInsertWidth(SI, outBW, -1, outBW, ITypeWidth,
          IOBW.Ext);
    } else {
      // The shift length is no constant int so the exact result width cannot
      // be determined.
      changed = forwardUpdateOrInsertWidth(SI, ITypeWidth, -1, ITypeWidth,
          ITypeWidth, OpIT->second.Ext);
    }
  } else {
    // The operand that should be shifted is not yet available
    changed = forwardUpdateOrInsertWidth(SI, ITypeWidth, -1, ITypeWidth,
        ITypeWidth, Loopus::ExtKind::ZExt);
  }
  printDBG(SI);
  return changed;
}

bool BitWidthAnalysis::forwardHandleShr(const BinaryOperator *SI,
    Loopus::ExtKind LeadingExt) {
  if (SI == 0) { return false; }

  bool changed = false;
  const int ITypeWidth = DL->getTypeSizeInBits(SI->getType());

  const Value *Op = SI->getOperand(0);
  BitWidthMapTy::iterator OpIT = BWMap.find(Op);
  if ((OpIT != BWMap.end()) && (OpIT->second.Valid == true)) {
    const struct BitWidth &IOBW = OpIT->second;

    const Value *SOp = SI->getOperand(1);
    if (isa<ConstantInt>(SOp) == true) {
      // Determine the shift length
      const ConstantInt *SLOp = dyn_cast<ConstantInt>(SOp);
      int ShiftLength = SLOp->getZExtValue();
      if (ShiftLength > ITypeWidth) {
        errs() << "WARNING: " << *SI << ": value shifted right more bits than "
            << "allowed by type width results in undefined behaviour!\n";
      }
      // This is a bit more complicated (similiar to the SExt and ZExt
      // instructions): if the input operand is sext'ed and a LShr is performed
      // the first bits should be filled with zeros.
      // If the input is zext'ed and LShr is performed the output is as wide as
      // the input (as the shift length is unknown) and the zext-flag is set.
      if (IOBW.Ext != LeadingExt) {
        // The signal should first be extended to the input datatype width using
        // its ExtKind-flag. Then it will be shifted right by sext/zext'ing
        // according to LeadingExt. So the output signal will have the width of
        // (output datatype - shift length):
        // %0 = SSSS XXXX XXXX  => i12, bw=8, ext=sext
        // %1 = lshr i12 %0, 3
        // %1 = 000S SSSX XXXX  => i12, bw=9, ext=zext
        const int usedOBW = (ITypeWidth > ShiftLength) ?
            ITypeWidth-ShiftLength : ITypeWidth;
        changed = forwardUpdateOrInsertWidth(SI, usedOBW, -1, usedOBW,
            ITypeWidth, LeadingExt);
      } else {
        // If the extension of the input signal and the shift operation are
        // equal then the output result has a width of
        // (input signal biwidth - shift length). The extension flag is set
        // according to the performed shift operation.
        int usedOBW = ITypeWidth;
        if ((IOBW.Valid == true) && (IOBW.OutBitwidth > 0)) {
          if (IOBW.OutBitwidth > ShiftLength) {
            usedOBW = IOBW.OutBitwidth - ShiftLength;
          }
        }
        changed = forwardUpdateOrInsertWidth(SI, usedOBW, -1, usedOBW,
            ITypeWidth, LeadingExt);
      }
    } else {
      // This is also a bit more complicated (similiar to the SExt and ZExt
      // instructions): if the input operand is sext'ed and a LShr is performed
      // the first bits should be filled with zeros. So the output signal will
      // have the width of the output datatype else no proper ExtKind-flag could
      // set as we would need a ZSExt flag.
      // If the input is zext'ed and LShr is performed the output is as wide as
      // the input (as the shift length is unknown) and the zext-flag is set.
      if (IOBW.Ext != LeadingExt) {
        changed = forwardUpdateOrInsertWidth(SI, ITypeWidth, -1, ITypeWidth,
            ITypeWidth, LeadingExt);
      } else {
        changed = forwardUpdateOrInsertWidth(SI, IOBW.OutBitwidth, -1,
            IOBW.OutBitwidth, ITypeWidth, LeadingExt);
      }
    }
  } else {
    // The operand that should be shifted is not yet available
    changed = forwardUpdateOrInsertWidth(SI, ITypeWidth, -1, ITypeWidth,
        ITypeWidth, LeadingExt);
  }
  printDBG(SI);
  return changed;
}

//===- Cast instructions --------------------------------------------------===//
// For extending casts:
// The input operands first have to be extended to the length of the input type
// width. That extended signal will then be used as ouput operand and carry the
// extend-flag depending on the instruction.
// For truncating casts:
// The input has to be set to the width of the output operand: if the input
// signal is narrower it has to be extended to the width of the output signal
// according to the ExtKind-flag of the input signal. The result will then carry
// a sext-flag. If the input signal is wider than the output it has to be
// truncated to the width of the output signal and will also carry the sext-flag.
bool BitWidthAnalysis::forwardHandleTrunc(const TruncInst *TI) {
  if (TI == 0) { return false; }

  const int ITypeWidth = DL->getTypeSizeInBits(TI->getType());
  const int DestTyBW = DL->getTypeSizeInBits(TI->getDestTy());
  // If the input signal width is really smaller than the output datatype
  // width the signal can just be forwarded. Else the input signal will be
  // truncated to the output datatype width with a SExt-flag to keep its
  // semantics.
  bool changed = false;
  const Value *Op = TI->getOperand(0);
  const BitWidthMapTy::const_iterator OpIT = BWMap.find(Op);
  if ((OpIT != BWMap.end()) && (OpIT->second.Valid == true)) {
    const struct BitWidth &IOBW = OpIT->second;
    if ((IOBW.OutBitwidth > 0) && (IOBW.OutBitwidth < DestTyBW)) {
      changed = forwardSetOrInsertWidth(TI, IOBW.OutBitwidth, -1,
          IOBW.OutBitwidth, ITypeWidth, IOBW.Ext);
    } else {
      changed = forwardSetOrInsertWidth(TI, DestTyBW, -1, DestTyBW, ITypeWidth,
          Loopus::ExtKind::SExt);
    }
  } else {
    changed = forwardSetOrInsertWidth(TI, DestTyBW, -1, DestTyBW, ITypeWidth,
        Loopus::ExtKind::SExt);
  }
  printDBG(TI);
  return changed;
}

// For ?Ext instructions the input signal is at most as wide as the requested
// input signal: if the input operand of a SExt-instruction is sext'ed too, it
// can simply be forwarded without modifications (so the signal width remains).
// If the input-operand of a SExt-instructions is zext'ed, the input has to be
// extended to the input datatype width and can then be used as output with a
// SExt-flag. The same holds vice versa (sext'ed signal as input for zext-inst).
bool BitWidthAnalysis::forwardHandleExt(const CastInst *EI,
    Loopus::ExtKind LeadingExt) {
  if (EI == 0) { return false; }

  bool changed = false;
  const int ITypeWidth = DL->getTypeSizeInBits(EI->getType());

  const Value *Op = EI->getOperand(0);
  BitWidthMapTy::iterator OpIT = BWMap.find(Op);
  if ((OpIT != BWMap.end()) && (OpIT->second.Valid == true)) {
    if (OpIT->second.Ext == LeadingExt) {
      // The input signal uses the same extension type as the ext-instruction
      // so it can simply be forwarded.
      const int InOpBW = OpIT->second.OutBitwidth;
      changed = forwardSetOrInsertWidth(EI, InOpBW, -1, InOpBW, ITypeWidth,
          LeadingExt);
    } else {
      // The input signal has a different extions kind than the ext-instruction.
      // So first extend signal to the required INPUT datatype width and use it
      // as output with the extension kind of the ext-instruction.
      const int SrcTyBW = DL->getTypeSizeInBits(EI->getSrcTy());
      changed = forwardSetOrInsertWidth(EI, SrcTyBW, -1, SrcTyBW, ITypeWidth,
          LeadingExt);
    }
  } else {
    // The input has not yet been processed. But at most the input can have as
    // many bits as the declared input datatype.
    const int SrcTyBW = DL->getTypeSizeInBits(EI->getSrcTy());
    changed = forwardSetOrInsertWidth(EI, SrcTyBW, -1, SrcTyBW, ITypeWidth,
        LeadingExt);
  }
  printDBG(EI);
  return changed;
}

bool BitWidthAnalysis::forwardHandleFPToI(const CastInst *CI,
    Loopus::ExtKind LeadingExt) {
  if (CI == 0) { return false; }

  const int ITypeWidth = DL->getTypeSizeInBits(CI->getType());
  const int DestTyBW = DL->getTypeSizeInBits(CI->getDestTy());
  // We do not know anything about the bitwidth as FP always have a fixed size
  bool changed = forwardSetOrInsertWidth(CI, DestTyBW, -1, DestTyBW,
      ITypeWidth, LeadingExt);
  printDBG(CI);
  return changed;
}

// The PtrToInt and IntToPtr instructions are a bit tricky as they are both a
// truncate and zero-extend instruction in one. If the source type is narrower
// than the pointer width a zero-extension is performed. If it is wider a
// truncation is done.
bool BitWidthAnalysis::forwardHandlePtrInt(const CastInst *PII) {
  if (PII == 0) { return false; }

  bool changed = false;
  const int ITypeWidth = DL->getTypeSizeInBits(PII->getType());
  const int SrcTyBW = DL->getTypeSizeInBits(PII->getSrcTy());
  const int DestTyBW = DL->getTypeSizeInBits(PII->getDestTy());

  // Collect input operand information
  const Value *Op = PII->getOperand(0);
  BitWidthMapTy::iterator OpIT = BWMap.find(Op);

  if (SrcTyBW > DestTyBW) {
    // The source type is wider than the destination type => truncate
    changed = forwardSetOrInsertWidth(PII, DestTyBW, -1, DestTyBW, ITypeWidth,
        Loopus::ExtKind::ZExt);

  } else if (SrcTyBW < DestTyBW) {
    // The source type is narrower than the destination type => zext
    if (OpIT != BWMap.end()) {
      if (OpIT->second.Ext == Loopus::ExtKind::ZExt) {
        // The input signal is already zero-extended so just forward the input
        const int InOpBW = OpIT->second.OutBitwidth;
        changed = forwardSetOrInsertWidth(PII, InOpBW, -1, InOpBW, ITypeWidth,
            Loopus::ExtKind::ZExt);
      } else {
        // The input is sign-extended so first extend the input to the required
        // INPUT datatype width (by extending with sign bit) and then use that
        // signal as output (having the ZExt-flag set)
        changed = forwardSetOrInsertWidth(PII, SrcTyBW, -1, SrcTyBW, ITypeWidth,
            Loopus::ExtKind::ZExt);
      }
    } else {
      // The input has not yet been processed. But at most the output can have
      // as many bits as the declared input datatype.
      changed = forwardSetOrInsertWidth(PII, SrcTyBW, -1, SrcTyBW, ITypeWidth,
          Loopus::ExtKind::ZExt);
    }

  } else {
    // Both have equal width => noop
    const struct BitWidth &IOBW = OpIT->second;
    changed = forwardSetOrInsertWidth(PII, IOBW.OutBitwidth, -1,
        IOBW.OutBitwidth, ITypeWidth, IOBW.Ext);
  }

  return changed;
}

bool BitWidthAnalysis::forwardHandleBitcast(const CastInst *BI) {
  if (BI == 0) { return false; }

  bool changed = false;
  const int ITypeWidth = DL->getTypeSizeInBits(BI->getType());

  // According to LLVM IR spec the bitcast instruction is noop: the input bits
  // are not modified.
  const Value *Op = BI->getOperand(0);
  BitWidthMapTy::iterator OpIT = BWMap.find(Op);
  if (OpIT != BWMap.end()) {
    const struct BitWidth &IOBW = OpIT->second;
    changed = forwardSetOrInsertWidth(BI, IOBW.OutBitwidth, -1, IOBW.OutBitwidth,
        ITypeWidth, IOBW.Ext);
  } else {
    // The input has not yet been processed. So the output datatype width
    // is used.
    const int DestTyBW = DL->getTypeSizeInBits(BI->getDestTy());
    changed = forwardSetOrInsertWidth(BI, DestTyBW, -1, DestTyBW, ITypeWidth,
        Loopus::ExtKind::ZExt);
  }
  printDBG(BI);
  return changed;
}

//===- Memory instructions ------------------------------------------------===//
bool BitWidthAnalysis::forwardHandleAlloca(const AllocaInst *AI) {
  if (AI == 0) { return false; }

  const int ITypeWidth = DL->getTypeSizeInBits(AI->getType());
  bool changed = forwardSetOrInsertWidth(AI, ITypeWidth, -1, ITypeWidth,
      ITypeWidth, Loopus::ExtKind::ZExt);
  printDBG(AI);
  return changed;
}

bool BitWidthAnalysis::forwardHandleLoad(const LoadInst *LI) {
  if (LI == 0) { return false; }

  // TODO: Not quite sure how to handle extension mode here...
  const int ITypeWidth = DL->getTypeSizeInBits(LI->getType());
  Loopus::ExtKind Extension = Loopus::ExtKind::Undef;
  if (LI->getType()->isFloatingPointTy() == true) {
    Extension = Loopus::ExtKind::FPNoExt;
  } else {
    Extension = Loopus::ExtKind::ZExt;
  }
  bool changed = forwardSetOrInsertWidth(LI, ITypeWidth, -1, ITypeWidth,
      ITypeWidth, Extension);
  printDBG(LI);
  return changed;
}

bool BitWidthAnalysis::forwardHandleGEP(const GetElementPtrInst *GEPI) {
  if (GEPI == 0) { return false; }

  const int ITypeWidth = DL->getTypeSizeInBits(GEPI->getType());
  bool changed = forwardSetOrInsertWidth(GEPI, ITypeWidth, -1, ITypeWidth,
      ITypeWidth, Loopus::ExtKind::ZExt);
  printDBG(GEPI);
  return changed;
}

//===- Misc instructions --------------------------------------------------===//
bool BitWidthAnalysis::forwardHandleCmp(const CmpInst *CI) {
  if (CI == 0) { return false; }

  const int ITypeWidth = DL->getTypeSizeInBits(CI->getType());
  bool changed = forwardSetOrInsertWidth(CI, 1, 1, 1, ITypeWidth,
      Loopus::ExtKind::ZExt);
  printDBG(CI);
  return changed;
}

bool BitWidthAnalysis::forwardHandleSelect(const Instruction *SI) {
  if (SI == 0) { return false; }

  const int ITypeWidth = DL->getTypeSizeInBits(SI->getType());
  // This is a bit tricky: there might two inputs with different widths and
  // different extension kinds. So if the inputs have both a sext- and a zext-
  // flag the output flag might be undetermined. To avoid we extend the signals
  // by one bit: the zext'ed operand prepends a zero and the sext'ed operand
  // prepends its sign bit: this result signal is then used as output signal
  // with a SExt-flag.

  int maxBW = -1;
  for (unsigned i = 0, e = SI->getNumOperands(); i < e; ++i) {
    const int OpBW = getBitWidth(SI->getOperand(i), true, SI);
    if (maxBW > 0) {
      if ((OpBW > 0) && (OpBW > maxBW)) {
        maxBW = OpBW;
      }
    } else {
      maxBW = OpBW;
    }
  }
  if (maxBW <= 0) {
    return false;
  }

  bool changed = forwardUpdateOrInsertWidth(SI, maxBW+1, -1, maxBW+1,
      ITypeWidth, Loopus::ExtKind::SExt);
  printDBG(SI);
  return changed;
}

bool BitWidthAnalysis::forwardHandleCall(const CallInst *CI) {
  if (CI == 0) { return false; }

  // TODO: Not quite sure how to handle extension mode here...
  if (CI->getType()->isSized() == true) {
    const int ITypeWidth = DL->getTypeSizeInBits(CI->getType());
    bool changed = forwardSetOrInsertWidth(CI, ITypeWidth, -1, ITypeWidth, ITypeWidth,
        Loopus::ExtKind::ZExt);
    printDBG(CI);
    return changed;
  } else {
    return false;
  }
}

//===- Floating-Point instructions ----------------------------------------===//
// Generic function for handling instructions that produce FP as result.
bool BitWidthAnalysis::forwardHandleFP(const Instruction *FPI) {
  if (FPI == 0) { return false; }

  const int ITypeWidth = DL->getTypeSizeInBits(FPI->getType());
  bool changed = forwardSetOrInsertWidth(FPI, ITypeWidth, -1, ITypeWidth, ITypeWidth,
      Loopus::ExtKind::FPNoExt);
  printDBG(FPI);
  return changed;
}

//===- Unrecognized instructions ------------------------------------------===//
// Generic function for handling instructions that are currently not recignized.
bool BitWidthAnalysis::forwardHandleDefault(const Instruction *I) {
  if (I == 0) { return false; }
  if (I->getType()->isSized() == true) {
    const int ITypeWidth = DL->getTypeSizeInBits(I->getType());
    bool changed = forwardSetOrInsertWidth(I, ITypeWidth, -1, ITypeWidth, ITypeWidth,
        Loopus::ExtKind::Undef);
    printDBG(I);
    return changed;
  } else {
    return false;
  }
}

bool BitWidthAnalysis::forwardPropagateBlock(const BasicBlock *BB) {
  if (BB == 0) { return false; }

  bool changed = false;

  for (BasicBlock::const_iterator INSIT = BB->begin(), INSEND = BB->end();
      INSIT != INSEND; ++INSIT) {
    const Instruction *I = &*INSIT;
    Value *VI = const_cast<Value*>(dyn_cast<Value>(I));
    if (SE->isSCEVable(VI->getType()) == true) {
      const SCEV *myscev = SE->getSCEV(VI);
      DEBUG(dbgs() << "NOW HANDLING: " << *I << "\n");
      DEBUG(dbgs() << "   SCEV: " << *myscev << "\n");
    }
    const unsigned IOpCode = I->getOpcode();
    switch (IOpCode) {
      // Arithmetic instructions
      case Instruction::BinaryOps::Add:
      case Instruction::BinaryOps::Sub:
        changed |= forwardHandleAddSub(dyn_cast<BinaryOperator>(I));
        break;
      case Instruction::BinaryOps::Mul:
        changed |= forwardHandleMul(dyn_cast<BinaryOperator>(I));
        break;
      case Instruction::BinaryOps::UDiv:
        changed |= forwardHandleUDiv(dyn_cast<BinaryOperator>(I));
        break;
      case Instruction::BinaryOps::SDiv:
        changed |= forwardHandleSDiv(dyn_cast<BinaryOperator>(I));
        break;
      case Instruction::BinaryOps::URem:
        changed |= forwardHandleURem(dyn_cast<BinaryOperator>(I));
        break;
      case Instruction::BinaryOps::SRem:
        changed |= forwardHandleURem(dyn_cast<BinaryOperator>(I));
        break;
      case Instruction::BinaryOps::FAdd:
      case Instruction::BinaryOps::FSub:
      case Instruction::BinaryOps::FMul:
      case Instruction::BinaryOps::FDiv:
      case Instruction::BinaryOps::FRem:
        changed |= forwardHandleFP(I);
        break;

      // Bitwise logical operations
      case Instruction::BinaryOps::Shl:
        changed |= forwardHandleShl(dyn_cast<BinaryOperator>(I));
        break;
      case Instruction::BinaryOps::LShr:
        changed |= forwardHandleShr(dyn_cast<BinaryOperator>(I),
            Loopus::ExtKind::ZExt);
        break;
      case Instruction::BinaryOps::AShr:
        changed |= forwardHandleShr(dyn_cast<BinaryOperator>(I),
            Loopus::ExtKind::SExt);
        break;
      case Instruction::BinaryOps::And:
        changed |= forwardHandleAnd(dyn_cast<BinaryOperator>(I));
        break;
      case Instruction::BinaryOps::Or:
      case Instruction::BinaryOps::Xor:
        changed |= forwardHandleOr(dyn_cast<BinaryOperator>(I));
        break;

      // Cast instructions
      case Instruction::CastOps::Trunc:
        changed |= forwardHandleTrunc(dyn_cast<TruncInst>(I));
        break;
      case Instruction::CastOps::ZExt:
        changed |= forwardHandleExt(dyn_cast<CastInst>(I),
            Loopus::ExtKind::ZExt);
        break;
      case Instruction::CastOps::SExt:
        changed |= forwardHandleExt(dyn_cast<CastInst>(I),
            Loopus::ExtKind::SExt);
        break;
      case Instruction::CastOps::FPToUI:
        changed |= forwardHandleFPToI(dyn_cast<CastInst>(I),
            Loopus::ExtKind::ZExt);
        break;
      case Instruction::CastOps::FPToSI:
        changed |= forwardHandleFPToI(dyn_cast<CastInst>(I),
            Loopus::ExtKind::SExt);
        break;
      case Instruction::CastOps::UIToFP:
      case Instruction::CastOps::SIToFP:
      case Instruction::CastOps::FPTrunc:
      case Instruction::CastOps::FPExt:
        changed |= forwardHandleFP(I);
        break;
      case Instruction::CastOps::PtrToInt:
      case Instruction::CastOps::IntToPtr:
        changed |= forwardHandlePtrInt(dyn_cast<CastInst>(I));
        break;
      case Instruction::CastOps::BitCast:
      case Instruction::CastOps::AddrSpaceCast:
        changed |= forwardHandleBitcast(dyn_cast<CastInst>(I));
        break;

      // Memory instructions
      case Instruction::MemoryOps::Alloca:
        changed |= forwardHandleAlloca(dyn_cast<AllocaInst>(I));
        break;
      case Instruction::MemoryOps::Load:
        changed |= forwardHandleLoad(dyn_cast<LoadInst>(I));
        break;
      case Instruction::MemoryOps::GetElementPtr:
        changed |= forwardHandleGEP(dyn_cast<GetElementPtrInst>(I));
        break;

      // Misc instructions
      case Instruction::OtherOps::ICmp:
      case Instruction::OtherOps::FCmp:
        changed |= forwardHandleCmp(dyn_cast<CmpInst>(I));
        break;
      case Instruction::OtherOps::Call:
        changed |= forwardHandleCall(dyn_cast<CallInst>(I));
        break;
      case Instruction::OtherOps::PHI:
      case Instruction::OtherOps::Select:
        changed |= forwardHandleSelect(I);
        break;

      // Unrecognized instructions
      default:
        changed |= forwardHandleDefault(I);
        break;
    }
  }

  return changed;
}

bool BitWidthAnalysis::forwardPropagate(Function &F) {
  typedef std::list<const BasicBlock*> WorklistTy;
  std::set<const BasicBlock*> VisitedBlocks;
  WorklistTy Worklist;

  bool changed = false;
  Worklist.push_back(&F.getEntryBlock());
  // Do a BFS over the basic blocks
  while (Worklist.empty() == false) {
    const BasicBlock *CurrentBlock = Worklist.front();
    Worklist.pop_front();

    // We do not want to process visited blocks again
    if (VisitedBlocks.count(CurrentBlock) > 0) {
      continue;
    }

    // Process the block
    changed |= forwardPropagateBlock(CurrentBlock);
    VisitedBlocks.insert(CurrentBlock);

    // Add all successors of the block
    for (succ_const_iterator SUCCIT = succ_begin(CurrentBlock),
        SUCCEND = succ_end(CurrentBlock); SUCCIT != SUCCEND; ++SUCCIT) {
      Worklist.push_back(*SUCCIT);
    }
  }

  return changed;
}

//===- Implementation of backward transition functions --------------------===//
//===----------------------------------------------------------------------===//
bool BitWidthAnalysis::backwardSetRequest(const Value *V, int newReqWidth) {
  if (V == 0) { return false; }

  if (isa<Constant>(V) == false) {
    const BitWidthMapTy::iterator VIT = BWMap.find(V);
    if (VIT != BWMap.end()) {
      // Just update the RequiredBitwidth if the requirement is larger then
      // existing one. So in the end the largest requirement should remain.
      if ((VIT->second.RequiredBitwidth > 0) && (newReqWidth > 0)) {
        if (newReqWidth > VIT->second.RequiredBitwidth) {
          VIT->second.RequiredBitwidth = newReqWidth;
          return true;
        }
      } else {
        VIT->second.RequiredBitwidth = newReqWidth;
        return true;
      }
    }
  }
  return false;
}

int BitWidthAnalysis::backwardHandleReqOrMaxop(const Instruction *AI) {
  if (AI == 0) { return -1; }

  // Determine the width required of the operands
  const BitWidthMapTy::iterator IIT = BWMap.find(AI);
  if ((IIT != BWMap.end())
   && (IIT->second.RequiredBitwidth >= 0)) {
    return IIT->second.RequiredBitwidth;
  } else {
    return getBitWidthLargestOp(AI, true);
  }
  return -1;
}

int BitWidthAnalysis::backwardHandleAddSub(const BinaryOperator *AI) {
  if (AI == 0) { return -1; }

  // Determine the width required of the operands
  const BitWidthMapTy::iterator IIT = BWMap.find(AI);
  if ((IIT != BWMap.end())
   && (IIT->second.RequiredBitwidth >= 0)) {
    return IIT->second.RequiredBitwidth;
  } else {
    return -1;
  }
  return -1;
}

int BitWidthAnalysis::backwardHandleAnd(const BinaryOperator *AI) {
  if (AI == 0) { return -1; }

  // Determine the smallest constant, zext'ed operand. We cannot be wider than
  // that operand.
  int reqBW = -1;
  const BitWidthMapTy::iterator IIT = BWMap.find(AI);
  if ((IIT != BWMap.end())
   && (IIT->second.RequiredBitwidth >= 0)) {
    reqBW = IIT->second.RequiredBitwidth;
  }

  if (reqBW > 0) {
    // There is a required bitwidth for this operand. But the result cannot be
    // wider than the widest const, zext'ed operands width.
    int minConstZExtBW = -1;
    for (unsigned i = 0, e = AI->getNumOperands(); i < e; ++i) {
      const Value *CurOP = AI->getOperand(i);
      if (isa<Constant>(CurOP) == false) { continue; }

      const int curOpBW = getBitWidth(CurOP, false, AI);
      // Determine smallest operand over all ops
      if (minConstZExtBW > 0) {
        if ((curOpBW > 0) && (curOpBW < minConstZExtBW)) {
          minConstZExtBW = curOpBW;
        }
      } else {
        minConstZExtBW = curOpBW;
      }
    }

    if (minConstZExtBW > 0) {
      if (reqBW > minConstZExtBW) {
        return minConstZExtBW;
      } else {
        return reqBW;
      }
    } else {
      return reqBW;
    }
  } else if (reqBW == 0) {
    // No requirements for the result width. So we are using the smallest
    // operands width and propagate it to all other ops.
    int minZExtBW = -1;
    for (unsigned i = 0, e = AI->getNumOperands(); i < e; ++i) {
      const Value *CurOP = AI->getOperand(i);
      const int curOpBW = getBitWidth(CurOP, false, AI);
      // Determine smallest operand over all ops
      if (minZExtBW > 0) {
        if ((curOpBW > 0) && (curOpBW < minZExtBW)) {
          minZExtBW = curOpBW;
        }
      } else {
        minZExtBW = curOpBW;
      }
    }
    return minZExtBW;
  } else {
    // The maximum possiblew bitwidth should be computed. But the result cannot
    // be wider than the widest const, zext'ed operands width.
    int minConstZExtBW = -1;
    for (unsigned i = 0, e = AI->getNumOperands(); i < e; ++i) {
      const Value *CurOP = AI->getOperand(i);
      if (isa<Constant>(CurOP) == false) { continue; }

      const int curOpBW = getBitWidth(CurOP, false, AI);
      // Determine smallest operand over all ops
      if (minConstZExtBW > 0) {
        if ((curOpBW > 0) && (curOpBW < minConstZExtBW)) {
          minConstZExtBW = curOpBW;
        }
      } else {
        minConstZExtBW = curOpBW;
      }
    }

    if (minConstZExtBW > 0) {
      return minConstZExtBW;
    } else {
      return -1;
    }
  }
  return -1;
}

int BitWidthAnalysis::backwardHandleShl(const BinaryOperator *SI) {
  if (SI == 0) { return -1; }

  const BitWidthMapTy::iterator IIT = BWMap.find(SI);
  if (IIT == BWMap.end()) {
    return -1;
  }

  const Value *SOp = SI->getOperand(1);
  const int reqBW = IIT->second.RequiredBitwidth;
  if (reqBW > 0) {
    if (isa<ConstantInt>(SOp) == true) {
      // The shift length is known. So the input value should have
      // requiredWidth-ShiftLength number of bits. Others bits will not be used.
      const ConstantInt *CSOp = dyn_cast<ConstantInt>(SOp);
      const int ShiftLength = CSOp->getZExtValue();
      int SrcWidth = reqBW - ShiftLength;
      if (SrcWidth < 0) {
        errs() << "WARNING: " << *SI << ": only " << reqBW << " bits of result "
            << "are needed. So shifting source op by " << ShiftLength << " bits "
            << "left results in a zero value!\n";
        SrcWidth = 0;
      }
      return SrcWidth;
    } else {
      // The shift length is unknown so the required bitwidth is forwarded to be
      // able to handle shifts by zero bits.
      return reqBW;
    }
  } else if (reqBW == 0) {
    return 0;
  }
  return -1;
}

int BitWidthAnalysis::backwardHandleShr(const BinaryOperator *SI) {
  if (SI == 0) { return -1; }

  const BitWidthMapTy::iterator IIT = BWMap.find(SI);
  if (IIT == BWMap.end()) {
    return -1;
  }

  const Value *SOp = SI->getOperand(1);
  const int reqBW = IIT->second.RequiredBitwidth;
  if (reqBW > 0) {
    if (isa<ConstantInt>(SOp) == true) {
      // The shift length is known. So the input value should have
      // requiredWidth+ShiftLength number of bits.
      const ConstantInt *CSOp = dyn_cast<ConstantInt>(SOp);
      const int ShiftLength = CSOp->getZExtValue();
      return reqBW + ShiftLength;
    } else {
      // The shift length is unknown but as we are shifting right we required as
      // many bits as possible.
      return -1;
    }
  } else if (reqBW == 0) {
    return 0;
  }
  return -1;
}

int BitWidthAnalysis::backwardHandleTrunc(const TruncInst *TI) {
  if (TI == 0) { return -1; }

  const int DestTyBW = DL->getTypeSizeInBits(TI->getDestTy());
  const BitWidthMapTy::iterator IIT = BWMap.find(TI);
  if (IIT == BWMap.end()) {
    return -1;
  }

  if (IIT->second.Valid == true) {
    const int reqBW = IIT->second.RequiredBitwidth;
    if ((reqBW >= 0) && (DestTyBW > reqBW)) {
      return reqBW;
    } else {
      return DestTyBW;
    }
  } else {
    return -1;
  }

  return -1;
}

int BitWidthAnalysis::backwardHandleGeneric(const Instruction *I) {
  if (I == 0) { return -1; }

  const BitWidthMapTy::iterator IIT = BWMap.find(I);
  if (IIT != BWMap.end()) {
    const int reqBW = IIT->second.RequiredBitwidth;
    if (reqBW >= 0) {
      return reqBW;
    }
  }
  return -1;
}

bool BitWidthAnalysis::backwardPropagateBlock(const BasicBlock *BB) {
  if (BB == 0) { return false; }
  bool changed = false;

  for (BasicBlock::const_reverse_iterator INSIT = BB->rbegin(),
      INSEND = BB->rend(); INSIT != INSEND; ++INSIT) {
    const Instruction *SI = &*INSIT;
    DEBUG(dbgs() << "NOW HANDLING BACKWARDS: " << *SI << "\n");

    DEBUG(
      for (Value::const_use_iterator UIT = SI->use_begin(), UEND = SI->use_end();
          UIT != UEND; ++UIT) {
        const User *U = UIT->getUser();
        if (isa<Instruction>(U) == false) { continue; }
        dbgs() << "   usr: " << *dyn_cast<Instruction>(U) << "\n";
      }
    );

    int maxUReq = -1;
    for (Value::const_use_iterator UIT = SI->use_begin(), UEND = SI->use_end();
        UIT != UEND; ++UIT) {
      const User *U = UIT->getUser();
      if (isa<Instruction>(U) == false) { continue; }
      const Instruction *I = dyn_cast<Instruction>(U);

      DEBUG(dbgs() << "   pur: " << *I);

      int curUReq = -1;
      const unsigned IOpCode = I->getOpcode();
      switch(IOpCode) {
        // Terminator instructions
        case Instruction::TermOps::Br:
          curUReq = 1;
          break;
        case Instruction::TermOps::Ret:
        case Instruction::TermOps::Switch:
          curUReq = -1;
          break;

        // Arithmetic instructions
        case Instruction::BinaryOps::Add:
        case Instruction::BinaryOps::Sub:
          curUReq = backwardHandleAddSub(dyn_cast<BinaryOperator>(I));
          break;
        case Instruction::BinaryOps::FAdd:
        case Instruction::BinaryOps::FSub:
        case Instruction::BinaryOps::FMul:
        case Instruction::BinaryOps::FDiv:
        case Instruction::BinaryOps::FRem:
          curUReq = -1;
          break;
        // Defaulting for: Mul, UDiv, SDiv, URem, SRem

        // Bitwise logical operations
        case Instruction::BinaryOps::Shl:
          curUReq = backwardHandleShl(dyn_cast<BinaryOperator>(I));
          break;
        case Instruction::BinaryOps::AShr:
        case Instruction::BinaryOps::LShr:
          curUReq = backwardHandleShr(dyn_cast<BinaryOperator>(I));
          break;
        case Instruction::BinaryOps::And:
          curUReq = backwardHandleAnd(dyn_cast<BinaryOperator>(I));
          break;
        case Instruction::BinaryOps::Or:
        case Instruction::BinaryOps::Xor:
          curUReq = backwardHandleGeneric(I);
          break;

        // Cast instructions
        case Instruction::CastOps::Trunc:
          curUReq = backwardHandleTrunc(dyn_cast<TruncInst>(I));
          break;
        case Instruction::CastOps::ZExt:
        case Instruction::CastOps::SExt:
        case Instruction::CastOps::BitCast:
          curUReq = backwardHandleGeneric(I);
          break;
        // Defaulting for: FPToUI, FPToSI, UIToFP, SIToFP, FPTrunc, FPExp,
        // PtrToInt, AddrSpaceCast
        case Instruction::CastOps::IntToPtr:
          curUReq = DL->getTypeSizeInBits(I->getOperand(0)->getType());
          break;

        // Misc instructions
        case Instruction::OtherOps::PHI:
        case Instruction::OtherOps::Select:
          curUReq = backwardHandleReqOrMaxop(I);
          break;

        case Instruction::OtherOps::ICmp:
        case Instruction::OtherOps::FCmp:
        case Instruction::OtherOps::Call:
          curUReq = -1;
          break;

        default:
          curUReq = -1;
          break;
      }
      DEBUG(dbgs() << " => " << curUReq << "\n");

      if (curUReq >= 0) {
        if (curUReq > maxUReq) {
          maxUReq = curUReq;
        }
      } else {
        // There is at least the current user that requires the maximum
        // available bitwidth. So we do not want to limit the bitwidth.
        maxUReq = -1;
        break;
      }
    }

    if (maxUReq >= 0) {
      changed |= backwardSetRequest(SI, maxUReq);
    } else {
      changed |= backwardSetRequest(SI, -1);
    }
    printDBG(SI);
  }
  return changed;
}

bool BitWidthAnalysis::backwardPropagate(Function &F) {
  typedef std::list<const BasicBlock*> WorklistTy;
  std::set<const BasicBlock*> VisitedBlocks;
  WorklistTy Worklist;

  for (BitWidthMapTy::iterator BWIT = BWMap.begin(), BWEND = BWMap.end();
      BWIT != BWEND; ++BWIT) {
    BWIT->second.PrevIterRequiredBitwidth = BWIT->second.RequiredBitwidth;
    BWIT->second.RequiredBitwidth = 0;
  }

  bool changed = false;
  for (po_iterator<BasicBlock*> POIT = po_begin(&F.getEntryBlock()),
      POEND = po_end(&F.getEntryBlock()); POIT != POEND; ++POIT) {
    backwardPropagateBlock(*POIT);
  }

  for (BitWidthMapTy::iterator BWIT = BWMap.begin(), BWEND = BWMap.end();
      BWIT != BWEND; ++BWIT) {
    if (BWIT->second.RequiredBitwidth > 0) {
      if (BWIT->second.PrevIterRequiredBitwidth != BWIT->second.RequiredBitwidth) {
        changed = true;
      }
    }
  }

  return changed;
}

//===- Public interface ---------------------------------------------------===//
std::pair<int, Loopus::ExtKind> BitWidthAnalysis::getBitWidth(const Value *V,
    const Instruction *OwningI) {
  if (isa<Constant>(V) == true) {
    const Constant *C = dyn_cast<Constant>(V);
    ConstantBitWidthMapTy::iterator OpIT = ConstantBWMap.find(std::make_pair(C, OwningI));
    if (OpIT != ConstantBWMap.end()) {
      return std::make_pair(OpIT->second.OutBitwidth, OpIT->second.Ext);
    } else {
      return std::make_pair(-1, Loopus::ExtKind::Undef);
    }
  } else if (isa<Instruction>(V) == true) {
    BitWidthMapTy::iterator OpIT = BWMap.find(V);
    if (OpIT != BWMap.end()) {
      return std::make_pair(OpIT->second.OutBitwidth, OpIT->second.Ext);
    } else {
      return std::make_pair(-1, Loopus::ExtKind::Undef);
    }
  } else if (isa<Argument>(V) == true) {
    const int ITypeWidth = DL->getTypeSizeInBits(V->getType());
    return std::make_pair(ITypeWidth, Loopus::ExtKind::SExt);
  } else {
    return std::make_pair(-1, Loopus::ExtKind::Undef);
  }
}

//===- Implementation of LLVM pass ----------------------------------------===//
INITIALIZE_PASS_BEGIN(BitWidthAnalysis, "loopus-bitwidth", "Bitwidth analysis",  false, true)
INITIALIZE_PASS_DEPENDENCY(ScalarEvolution)
INITIALIZE_PASS_DEPENDENCY(DataLayoutPass)
INITIALIZE_PASS_END(BitWidthAnalysis, "loopus-bitwidth", "Bitwidth analysis",  false, true)

char BitWidthAnalysis::ID = 0;

namespace llvm {
  Pass* createBitWidthAnalysisPass() {
    return new BitWidthAnalysis();
  }
}

BitWidthAnalysis::BitWidthAnalysis(void)
 : FunctionPass(ID), SE(0), DL(0) {
  initializeBitWidthAnalysisPass(*PassRegistry::getPassRegistry());
}

void BitWidthAnalysis::printDBG(const Instruction *I) {
  if (I == 0) { return; }

  BitWidthMapTy::iterator IIT = BWMap.find(I);
  if (IIT == BWMap.end()) {
    DEBUG(dbgs() << *I << "\n    @" << I << " : <n.a.>" << "\n");
  } else {
    DEBUG(dbgs() << *I << "\n"
        << "    @" << I << " ; ";
        printBitWidth(dbgs(), IIT->second);
    );
  }
}

void BitWidthAnalysis::printAllDBG(const Function &F) {
  DEBUG(
    for (const_inst_iterator INSIT = inst_begin(F), INSEND = inst_end(F);
        INSIT != INSEND; ++INSIT) {
      const Instruction *I = &*INSIT;

      const auto ibw = this->getBitWidth(I);

      dbgs() << "INSTR   " << *I << "\n";
      dbgs() << "   @" << I << " : bw=" << ibw.first << ",";
      switch(ibw.second) {
        case Loopus::ExtKind::Undef:
          dbgs() << "udef";
          break;
        case Loopus::ExtKind::SExt:
          dbgs() << "sext";
          break;
        case Loopus::ExtKind::ZExt:
          dbgs() << "zext";
          break;
        case Loopus::ExtKind::OneExt:
          dbgs() << "oext";
          break;
        case Loopus::ExtKind::FPNoExt:
          dbgs() << "fpnone";
          break;
        default:
          dbgs() << "unknown";
          break;
      }
      dbgs() << "\n";

      for (User::const_op_iterator OPIT = I->op_begin(), OPEND = I->op_end();
          OPIT != OPEND; ++OPIT) {
        const Value *Op = OPIT->get();
        if (isa<BasicBlock>(Op) == true) {
          dbgs() << "   OP: " << Op->getName() << " @" << Op << "\n";
        } else if ((isa<Argument>(Op) == true) || (isa<User>(Op) == true)) {
          const auto opbw = this->getBitWidth(Op, I);
          dbgs() << "   OP: " << *Op << " @" << Op << " : bw=" << opbw.first << ",";
          switch(opbw.second) {
            case Loopus::ExtKind::Undef:
              dbgs() << "udef";
              break;
            case Loopus::ExtKind::SExt:
              dbgs() << "sext";
              break;
            case Loopus::ExtKind::ZExt:
              dbgs() << "zext";
              break;
            case Loopus::ExtKind::OneExt:
              dbgs() << "oext";
              break;
            case Loopus::ExtKind::FPNoExt:
              dbgs() << "fpnone";
              break;
            default:
              dbgs() << "unknown";
              break;
          }
          dbgs() << "\n";
        }
      }
    }
  );
}

void BitWidthAnalysis::printBitWidth(raw_ostream &O, const struct BitWidth &BW) const {
  O << "bw=" << BW.OutBitwidth;
  switch (BW.Ext) {
    case Loopus::ExtKind::Undef:
      O << ",udef";
      break;
    case Loopus::ExtKind::SExt:
      O << ",sext";
      break;
    case Loopus::ExtKind::ZExt:
      O << ",zext";
      break;
    case Loopus::ExtKind::OneExt:
      O << ",oext";
      break;
    case Loopus::ExtKind::FPNoExt:
      O << ",fpnone";
      break;
    default:
      O << ",unknown";
      break;
  }
  O << " ; tyw=" << BW.TypeWidth;
  O << "/" << BW.OutBitwidth - BW.TypeWidth;
  O << " ; urq=" << BW.RequiredBitwidth;
  O << " ; prq=" << BW.PrevIterRequiredBitwidth;
  O << " ; vbm=" << BW.ValueMaskBitwidth;
  O << "\n";
}

void BitWidthAnalysis::print(raw_ostream &O, const Module *M) const {
  typedef std::set<const Function*> FuncSetTy;
  FuncSetTy funcs;

  if (M != 0) {
    // The processed module is available so collect all functions in it
    for (Module::const_iterator FIT = M->begin(), FEND = M->end();
        FIT != FEND; ++FIT) {
      funcs.insert(&*FIT);
    }
  } else {
    // The processed module is not available so iterate over all processed
    // instructions and collect all different functions
    for (BitWidthMapTy::const_iterator INSIT = BWMap.cbegin(),
        INSEND = BWMap.cend(); INSIT != INSEND; ++INSIT) {
      const Value *V = INSIT->first;
      const Function *F = 0;
      if (isa<Instruction>(V) == true) {
        F = dyn_cast<Instruction>(V)->getParent()->getParent();
      } else if (isa<Argument>(V) == true) {
        F = dyn_cast<Argument>(V)->getParent();
      } else if (isa<BasicBlock>(V) == true) {
        F = dyn_cast<BasicBlock>(V)->getParent();
      }
      if ((F != 0) && (funcs.count(F) == 0)) {
        funcs.insert(F);
      }
    }
  }

  int alloverSavingsInsts = 0;
  // Now iterate over all collected functions
  for (FuncSetTy::const_iterator FIT = funcs.cbegin(), FEND = funcs.cend();
      FIT != FEND; ++FIT) {
    for (const_inst_iterator FINSIT = inst_begin(*FIT), FINSEND = inst_end(*FIT);
        FINSIT != FINSEND; ++FINSIT) {
      // Fetch instruction
      const Instruction *I = &*FINSIT;
      const BitWidthMapTy::const_iterator IIT = BWMap.find(I);
      O << "INSTR @" << I << ": " << *I << "\n";
      if (IIT == BWMap.cend()) {
        O << "   <no info available>\n";
        continue;
      } else {
        O << "   ";
        printBitWidth(O, IIT->second);
        alloverSavingsInsts += (IIT->second.OutBitwidth - IIT->second.TypeWidth);
      }
    }
  }

  int alloverSavingsConsts = 0;
  // Print width of all constants
  for (ConstantBitWidthMapTy::const_iterator CIT = ConstantBWMap.cbegin(),
      CEND = ConstantBWMap.cend(); CIT != CEND; ++CIT) {
    const std::pair<const Constant*, const Instruction*> &curC = CIT->first;
    O << "CONST @" << curC.first << ": " << *curC.first << "\n";
    O << "   inst: @" << curC.second << ": " << *curC.second << "\n";
    O << "   ";
    if (isa<ConstantInt>(curC.first) == true) {
      O << "ty=cint";
      const ConstantInt *CINT = dyn_cast<ConstantInt>(curC.first);
      O << " ; svl=" << CINT->getSExtValue();
      O << " ; uvl=" << CINT->getZExtValue();
    } else if (isa<ConstantFP>(curC.first) == true) {
      O << "ty=cfp";
      const ConstantFP * CFP = dyn_cast<ConstantFP>(curC.first);
      O << " ; dvl=" << CFP->getValueAPF().convertToDouble();
      O << " ; dfl=" << CFP->getValueAPF().convertToFloat();
    } else {
      O << "ty=na";
    }
    O << "\n   ";
    printBitWidth(O, CIT->second);
    alloverSavingsConsts += (CIT->second.OutBitwidth - CIT->second.TypeWidth);
  }

  O << "SAVES\n";
  O << "   insts=" << alloverSavingsInsts << "\n";
  O << "   const=" << alloverSavingsConsts << "\n";
}

void BitWidthAnalysis::getAnalysisUsage(AnalysisUsage &AU) const {
  AU.addRequired<ScalarEvolution>();
  AU.addRequired<DataLayoutPass>();
  AU.setPreservesAll();
}

bool BitWidthAnalysis::runOnFunction(Function &F) {
  // A dual pass thing is used: first the CFG is traversed in forward direction
  // to propagate information along then nodes and then it traversed backwards
  // to propagate the requirements to all predecessors.
  SE = &getAnalysis<ScalarEvolution>();
  DL = &getAnalysis<DataLayoutPass>().getDataLayout();

  bool changed = true;
  do {
    changed = false;
    changed |= forwardPropagate(F);
    changed |= backwardPropagate(F);
    ++StatsNumIterations;
  } while (changed == true);
  printAllDBG(F);

  return false;
}

