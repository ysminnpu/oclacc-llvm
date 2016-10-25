//===- ArgPromotionTracker.cpp --------------------------------------------===//
//===----------------------------------------------------------------------===//

#define DEBUG_TYPE "loopus-argpromtracker"

#include "ArgPromotionTracker.h"

#include "llvm/IR/Constants.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/Value.h"
#include "llvm/Support/Casting.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/ErrorHandling.h"

using namespace llvm;

//===- Low-level metadata list access -------------------------------------===//
std::string ArgPromotionTracker::resolveBIFToString(const BuiltInFunctionCall::BuiltInFunction BIF) const {
  switch (BIF) {
    case BuiltInFunctionCall::BuiltInFunction::BIF_Undefined:
      return Loopus::PromotedArgStrings::OCLACC_BIF_UNDEF;
    case BuiltInFunctionCall::BuiltInFunction::BIF_GetWorkDim:
      return Loopus::PromotedArgStrings::OCLACC_BIF_WDIM;
    case BuiltInFunctionCall::BuiltInFunction::BIF_GetGlobalSize:
      return Loopus::PromotedArgStrings::OCLACC_BIF_GLOBSZ;
    case BuiltInFunctionCall::BuiltInFunction::BIF_GetGlobalID:
      return Loopus::PromotedArgStrings::OCLACC_BIF_GLOBID;
    case BuiltInFunctionCall::BuiltInFunction::BIF_GetLocalSize:
      return Loopus::PromotedArgStrings::OCLACC_BIF_LOCSZ;
    case BuiltInFunctionCall::BuiltInFunction::BIF_GetEnqLocalSize:
      return Loopus::PromotedArgStrings::OCLACC_BIF_ENQLOCSZ;
    case BuiltInFunctionCall::BuiltInFunction::BIF_GetLocalID:
      return Loopus::PromotedArgStrings::OCLACC_BIF_LOCID;
    case BuiltInFunctionCall::BuiltInFunction::BIF_GetNumGroups:
      return Loopus::PromotedArgStrings::OCLACC_BIF_NUMGROUPS;
    case BuiltInFunctionCall::BuiltInFunction::BIF_GetGroupID:
      return Loopus::PromotedArgStrings::OCLACC_BIF_GROUPID;
    case BuiltInFunctionCall::BuiltInFunction::BIF_GetGlobalOffset:
      return Loopus::PromotedArgStrings::OCLACC_BIF_GLOBOFF;
    case BuiltInFunctionCall::BuiltInFunction::BIF_GetGlobLinearID:
      return Loopus::PromotedArgStrings::OCLACC_BIF_GLOBLINID;
    case BuiltInFunctionCall::BuiltInFunction::BIF_GetLocLinearID:
      return Loopus::PromotedArgStrings::OCLACC_BIF_LOCLINID;
  }
  return Loopus::PromotedArgStrings::OCLACC_BIF_UNDEF;
}

BuiltInFunctionCall::BuiltInFunction ArgPromotionTracker::resolveStringToBIF(const std::string &BIFIdent) const {
  if (BIFIdent == Loopus::PromotedArgStrings::OCLACC_BIF_UNDEF) {
    return BuiltInFunctionCall::BuiltInFunction::BIF_Undefined;
  } else if (BIFIdent == Loopus::PromotedArgStrings::OCLACC_BIF_WDIM) {
    return BuiltInFunctionCall::BuiltInFunction::BIF_GetWorkDim;
  } else if (BIFIdent == Loopus::PromotedArgStrings::OCLACC_BIF_GLOBSZ) {
    return BuiltInFunctionCall::BuiltInFunction::BIF_GetGlobalSize;
  } else if (BIFIdent == Loopus::PromotedArgStrings::OCLACC_BIF_GLOBID) {
    return BuiltInFunctionCall::BuiltInFunction::BIF_GetGlobalID;
  } else if (BIFIdent == Loopus::PromotedArgStrings::OCLACC_BIF_LOCSZ) {
    return BuiltInFunctionCall::BuiltInFunction::BIF_GetLocalSize;
  } else if (BIFIdent == Loopus::PromotedArgStrings::OCLACC_BIF_ENQLOCSZ) {
    return BuiltInFunctionCall::BuiltInFunction::BIF_GetEnqLocalSize;
  } else if (BIFIdent == Loopus::PromotedArgStrings::OCLACC_BIF_LOCID) {
    return BuiltInFunctionCall::BuiltInFunction::BIF_GetLocalID;
  } else if (BIFIdent == Loopus::PromotedArgStrings::OCLACC_BIF_NUMGROUPS) {
    return BuiltInFunctionCall::BuiltInFunction::BIF_GetNumGroups;
  } else if (BIFIdent == Loopus::PromotedArgStrings::OCLACC_BIF_GROUPID) {
    return BuiltInFunctionCall::BuiltInFunction::BIF_GetGroupID;
  } else if (BIFIdent == Loopus::PromotedArgStrings::OCLACC_BIF_GLOBOFF) {
    return BuiltInFunctionCall::BuiltInFunction::BIF_GetGlobalOffset;
  } else if (BIFIdent == Loopus::PromotedArgStrings::OCLACC_BIF_GLOBLINID) {
    return BuiltInFunctionCall::BuiltInFunction::BIF_GetGlobLinearID;
  } else if (BIFIdent == Loopus::PromotedArgStrings::OCLACC_BIF_LOCLINID) {
    return BuiltInFunctionCall::BuiltInFunction::BIF_GetLocLinearID;
  } else {
    return BuiltInFunctionCall::BuiltInFunction::BIF_Undefined;
  }
}

// Some low-level functions to access the list of promoted arguments in the
// metadata. The list is stored in a named metadata node. Each entry in that
// node is a list with the following structure:
// 1. The index of the promoted argument within the functions parameter list
// 2. The function owning the argument
// 3. An identifier string representing the originally called id-function
// 4. A constant representing the argument provided to the call (if used). Note
//    that all those call arguments should be of the same type, that is stored
//    in the MDCallArgType pointer.

Argument* ArgPromotionTracker::getArgByIndex(Function *Func, Constant *Index) const {
  if ((Func == nullptr) || (Index == nullptr)) { return nullptr; }
  ConstantInt *IndexCI = dyn_cast<ConstantInt>(Index);
  if (IndexCI == nullptr) { return nullptr; }
  const unsigned ReqArgIdx = IndexCI->getZExtValue();
  if (ReqArgIdx >= Func->arg_size()) { return nullptr; }
  Function::arg_iterator AIT = Func->arg_begin(), AEND = Func->arg_end();
  for (unsigned i = 0; i < ReqArgIdx; ++i) {
    if (AIT != AEND) {
      ++AIT;
    }
  }
  if (AIT == AEND) { return nullptr; }
  return &*AIT;
}

Constant* ArgPromotionTracker::getArgIndex(Argument *Arg) const {
  if (Arg == nullptr) { return nullptr; }
  const unsigned ArgIdx = Arg->getArgNo();
  ConstantInt *ArgIdxCI = ConstantInt::get(MDCallArgType, ArgIdx, false);
  return ArgIdxCI;
}

MDNode* ArgPromotionTracker::findPromotedArgEntry(Argument *Arg) const {
  if (Arg == nullptr) { return nullptr; }

  Module *M = Arg->getParent()->getParent();
  NamedMDNode *MDPromArgList = M->getNamedMetadata(Loopus::KernelMDStrings::OCLACC_PROMARGLIST);
  if (MDPromArgList == nullptr) {
    // There is no list of promoted args available
    return nullptr;
  }
  if (MDPromArgList->getNumOperands() == 0) { return nullptr; }

  Constant* const ArgIdx = getArgIndex(Arg);

  // Now check all entries
  for (unsigned i = 0, e = MDPromArgList->getNumOperands(); i < e; ++i) {
    MDNode *MDPromArgEntry = MDPromArgList->getOperand(i);
    if (MDPromArgEntry == nullptr) { continue; }
    // We check for less than four to be prepared for more than one argument to
    // an ID builtin function
    if (MDPromArgEntry->getNumOperands() < 2) { continue; }

    // Check for correct index
    Metadata *MDPAEArg = MDPromArgEntry->getOperand(0).get();
    ValueAsMetadata *MDPAEArgVal = dyn_cast<ValueAsMetadata>(MDPAEArg);
    if (MDPAEArgVal == nullptr) { continue; }
    Value *ArgVal = MDPAEArgVal->getValue();
    if (ArgVal != ArgIdx) { continue; }

    // Check for correct function
    if (Arg->getParent() != nullptr) {
      Metadata *MDPAEFunc = MDPromArgEntry->getOperand(1).get();
      ValueAsMetadata *MDPAEFuncVal = dyn_cast<ValueAsMetadata>(MDPAEFunc);
      if (MDPAEFuncVal == nullptr) { continue; }
      Value *FuncVal = MDPAEFuncVal->getValue();
      if (FuncVal != Arg->getParent()) { continue; }
    } else {
      return nullptr;
    }

    return MDPromArgEntry;
  }

  return nullptr;
}

/// Adds an entry for the given argument to the metadata list assuming that it
/// was promoted from a function call to the function represented by BIF with
/// the given argument.
bool ArgPromotionTracker::addPromotedArgEntry(Argument *Arg,
    BuiltInFunctionCall::BuiltInFunction BIF, Constant *BIFArgument) {
  if (Arg == nullptr) { return false; }
  if (Arg->getParent() == nullptr) { return false; }
  if (BIFArgument != nullptr) {
    if (BIFArgument->getType() != MDCallArgType) {
      return false;
    }
  }

  Module *M = Arg->getParent()->getParent();
  NamedMDNode *MDPromArgList = M->getOrInsertNamedMetadata(Loopus::KernelMDStrings::OCLACC_PROMARGLIST);
  Constant* const ArgIdx = getArgIndex(Arg);
  if (ArgIdx == nullptr) { return false; }

  if (MDPromArgList != nullptr) {
    // First create the MDNode for the argument
    SmallVector<Metadata*, 4> PAEContents;
    ValueAsMetadata *MDPAEArgVal = ValueAsMetadata::get(ArgIdx);
    PAEContents.push_back(MDPAEArgVal);
    ValueAsMetadata *MDPAEFuncVal = ValueAsMetadata::get(Arg->getParent());
    PAEContents.push_back(MDPAEFuncVal);
    MDString *MDPAEBIFString = MDString::get(getGlobalContext(), resolveBIFToString(BIF));
    PAEContents.push_back(MDPAEBIFString);
    if (BIFArgument != nullptr) {
      ConstantAsMetadata *MDPAECallArgConst = ConstantAsMetadata::get(BIFArgument);
      PAEContents.push_back(MDPAECallArgConst);
    }
    MDNode *MDPromArgEntry = MDNode::get(getGlobalContext(), PAEContents);

    // Now add the created entry into the list
    if (MDPromArgEntry != nullptr) {
      MDPromArgList->addOperand(MDPromArgEntry);
      return true;
    }
  }
  return false;
}

bool ArgPromotionTracker::removePromotedArgEntry(Argument *Arg) {
  if (Arg == nullptr) { return false; }
  if (Arg->getParent() == nullptr) { return false; }

  Module *M = Arg->getParent()->getParent();
  NamedMDNode *MDPromArgList = M->getNamedMetadata(Loopus::KernelMDStrings::OCLACC_PROMARGLIST);
  if (MDPromArgList == nullptr) {
    // There is no list of promoted args available
    return false;
  }
  if (MDPromArgList->getNumOperands() == 0) { return false; }

  Constant* const ArgIdx = getArgIndex(Arg);

  // Now check all entries
  SmallVector<MDNode*, 8> KeptMDNodes;
  for (unsigned i = 0, e = MDPromArgList->getNumOperands(); i < e; ++i) {
    MDNode *MDPromArgEntry = MDPromArgList->getOperand(i);
    if (MDPromArgEntry == nullptr) { continue; }
    // We check for less than four to be prepared for more than one argument to
    // an ID builtin function
    if (MDPromArgEntry->getNumOperands() < 2) {
      KeptMDNodes.push_back(MDPromArgEntry);
    }

    // Extract the argument index
    Metadata *MDPAEArg = MDPromArgEntry->getOperand(0).get();
    ValueAsMetadata *MDPAEArgVal = dyn_cast<ValueAsMetadata>(MDPAEArg);
    if (MDPAEArgVal == nullptr) {
      KeptMDNodes.push_back(MDPromArgEntry);
      continue;
    }
    Value *ArgVal = MDPAEArgVal->getValue();
    // Extract the function
    Metadata *MDPAEFunc = MDPromArgEntry->getOperand(1).get();
    ValueAsMetadata *MDPAEFuncVal = dyn_cast<ValueAsMetadata>(MDPAEFunc);
    if (MDPAEFuncVal == nullptr) {
      KeptMDNodes.push_back(MDPromArgEntry);
      continue;
    }
    Value *FuncVal = MDPAEFuncVal->getValue();
    if ((ArgVal != ArgIdx) || (FuncVal != Arg->getParent())) {
      KeptMDNodes.push_back(MDPromArgEntry);
    }
  }

  M->eraseNamedMetadata(MDPromArgList); MDPromArgList = nullptr;
  MDPromArgList = M->getOrInsertNamedMetadata(Loopus::KernelMDStrings::OCLACC_PROMARGLIST);
  if (MDPromArgList != nullptr) {
    for (MDNode *OldMDN : KeptMDNodes) {
      MDPromArgList->addOperand(OldMDN);
    }
  }

  return true;
}

unsigned ArgPromotionTracker::removePromotedArgEntriesForFunction(Function *Func) {
  if (Func == nullptr) { return 0; }

  Module *M = Func->getParent();
  NamedMDNode *MDPromArgList = M->getNamedMetadata(Loopus::KernelMDStrings::OCLACC_PROMARGLIST);
  if (MDPromArgList == nullptr) {
    // There is no list of promoted args available
    return false;
  }
  if (MDPromArgList->getNumOperands() == 0) { return false; }

  // Now check all entries
  SmallVector<MDNode*, 8> KeptMDNodes;
  for (unsigned i = 0, e = MDPromArgList->getNumOperands(); i < e; ++i) {
    MDNode *MDPromArgEntry = MDPromArgList->getOperand(i);
    if (MDPromArgEntry == nullptr) { continue; }
    // We check for less than four to be prepared for more than one argument to
    // an ID builtin function
    if (MDPromArgEntry->getNumOperands() < 2) {
      KeptMDNodes.push_back(MDPromArgEntry);
      continue;
    }

    Metadata *MDPAEFunc = MDPromArgEntry->getOperand(1).get();
    ValueAsMetadata *MDPAEFuncVal = dyn_cast<ValueAsMetadata>(MDPAEFunc);
    if (MDPAEFuncVal == nullptr) {
      KeptMDNodes.push_back(MDPromArgEntry);
      continue;
    }
    Value *FuncVal = MDPAEFuncVal->getValue();
    if (FuncVal != Func) {
      KeptMDNodes.push_back(MDPromArgEntry);
    }
  }

  M->eraseNamedMetadata(MDPromArgList); MDPromArgList = nullptr;
  MDPromArgList = M->getOrInsertNamedMetadata(Loopus::KernelMDStrings::OCLACC_PROMARGLIST);
  if (MDPromArgList != nullptr) {
    for (MDNode *OldMDN : KeptMDNodes) {
      MDPromArgList->addOperand(OldMDN);
    }
  }
  return 0;
}

//===- High level API functions -------------------------------------------===//
bool ArgPromotionTracker::isPromotedArgument(const Argument *Arg) const {
  if (Arg == nullptr) { return false; }

  const Function *ArgFunc = Arg->getParent();
  if (ArgFunc == nullptr) { return false; }
  const MapTy::const_iterator FuncPosition = PromotedArgs.find(const_cast<Function*>(ArgFunc));
  if (FuncPosition == PromotedArgs.cend()) {
    // The owning function does not have any promoted arguments
    return false;
  } else {
    for (ArgListTy::const_iterator ALIT = FuncPosition->second.begin(),
        ALEND = FuncPosition->second.end(); ALIT != ALEND; ++ALIT) {
      if ((ALIT->Argument == Arg) && (ALIT->Function == ArgFunc)) {
        return true;
      }
    }
  }

  return false;
}

Argument* ArgPromotionTracker::getPromotedArgument(const Function *Func,
    const BuiltInFunctionCall::BuiltInFunction BIF, const Constant *CallArg) const {
  if (Func == nullptr) { return nullptr; }

  const MapTy::const_iterator FuncPosition = PromotedArgs.find(const_cast<Function*>(Func));
  if (FuncPosition == PromotedArgs.cend()) {
    // There are promoted arguments for the requested function
    return nullptr;
  } else {
    for (ArgListTy::const_iterator ALIT = FuncPosition->second.begin(),
        ALEND = FuncPosition->second.end(); ALIT != ALEND; ++ALIT) {
      if ((ALIT->Function == Func) && (ALIT->BIF == BIF) && (ALIT->CallArg == CallArg)) {
        return ALIT->Argument;
      }
    }
  }

  return nullptr;
}

bool ArgPromotionTracker::hasPromotedArgument(const Function *Func,
    const BuiltInFunctionCall::BuiltInFunction BIF, const long CallArg) const {
  ConstantInt *CallArgCI = ConstantInt::get(MDCallArgType, CallArg, true);
  if (getPromotedArgument(Func, BIF, CallArgCI) == nullptr) {
    return false;
  } else {
    return true;
  }
}

bool ArgPromotionTracker::hasPromotedArgument(const Function *Func,
    const BuiltInFunctionCall::BuiltInFunction BIF) const {
  if (getPromotedArgument(Func, BIF, nullptr) == nullptr) {
    return false;
  } else {
    return true;
  }
}

Argument* ArgPromotionTracker::getPromotedArgument(const Function *Func,
    const BuiltInFunctionCall::BuiltInFunction BIF, const long CallArg) const {
  ConstantInt *CallArgCI = ConstantInt::get(MDCallArgType, CallArg, true);
  return getPromotedArgument(Func, BIF, CallArgCI);
}

Argument* ArgPromotionTracker::getPromotedArgument(const Function *Func,
    const BuiltInFunctionCall::BuiltInFunction BIF) const {
  return getPromotedArgument(Func, BIF, nullptr);
}

unsigned ArgPromotionTracker::getAllPromotedArgumentsList(const Function *Func,
    SmallVectorImpl<Argument*> &ArgList) {
  if (Func == nullptr) { return 0; }

  unsigned GatheredArgs = 0;
  const MapTy::const_iterator FuncPosition = PromotedArgs.find(const_cast<Function*>(Func));
  if (FuncPosition == PromotedArgs.cend()) {
    // There are promoted arguments for the requested function
    return 0;
  } else {
    for (ArgListTy::const_iterator ALIT = FuncPosition->second.begin(),
        ALEND = FuncPosition->second.end(); ALIT != ALEND; ++ALIT) {
      if (ALIT->Function == Func) {
        ArgList.push_back(ALIT->Argument);
        ++GatheredArgs;
      }
    }
  }

  return GatheredArgs;
}

unsigned ArgPromotionTracker::getPromotedArgumentsList(const Function *Func,
    BuiltInFunctionCall::BuiltInFunction BIF, SmallVectorImpl<Argument*> &ArgList) {
  if (Func == nullptr) { return 0; }

  unsigned GatheredArgs = 0;
  const MapTy::const_iterator FuncPosition = PromotedArgs.find(const_cast<Function*>(Func));
  if (FuncPosition == PromotedArgs.cend()) {
    // There are promoted arguments for the requested function
    return 0;
  } else {
    for (ArgListTy::const_iterator ALIT = FuncPosition->second.begin(),
        ALEND = FuncPosition->second.end(); ALIT != ALEND; ++ALIT) {
      if ((ALIT->Function == Func) && (ALIT->BIF == BIF)) {
        ArgList.push_back(ALIT->Argument);
        ++GatheredArgs;
      }
    }
  }

  return GatheredArgs;
}

Constant* ArgPromotionTracker::getCallArgForPromotedArgument_Int(const Argument *Arg) {
  if (Arg == nullptr) { return nullptr; }

  Function *ArgFunc = const_cast<Function*>(Arg->getParent());
  const MapTy::iterator FuncPosition = PromotedArgs.find(ArgFunc);
  if (FuncPosition == PromotedArgs.cend()) {
    // There are promoted arguments for the requested function
    return nullptr;
  } else {
    for (ArgListTy::const_iterator ALIT = FuncPosition->second.begin(),
        ALEND = FuncPosition->second.end(); ALIT != ALEND; ++ALIT) {
      if ((ALIT->Argument == Arg) && (ALIT->Function == ArgFunc)) {
        return ALIT->CallArg;
      }
    }
  }

  return nullptr;
}

bool ArgPromotionTracker::hasCallArgForPromotedArgument(const Argument *Arg) {
  Constant *CallArg = getCallArgForPromotedArgument_Int(Arg);
  if (CallArg != nullptr) {
    return true;
  } else {
    return false;
  }
}

long ArgPromotionTracker::getCallArgForPromotedArgument(const Argument *Arg) {
  Constant *CallArg = getCallArgForPromotedArgument_Int(Arg);
  if (CallArg != nullptr) {
    if (isa<ConstantInt>(CallArg) == true) {
      ConstantInt *CallArgCI = dyn_cast<ConstantInt>(CallArg);
      return CallArgCI->getSExtValue();
    }
  }
  return 0;
}

BuiltInFunctionCall::BuiltInFunction ArgPromotionTracker::getBIFForPromotedArgument(
    const Argument *Arg) {
  if (Arg == nullptr) {
    return BuiltInFunctionCall::BuiltInFunction::BIF_Undefined;
  }

  Function *ArgFunc = const_cast<Function*>(Arg->getParent());
  const MapTy::iterator FuncPosition = PromotedArgs.find(ArgFunc);
  if (FuncPosition == PromotedArgs.cend()) {
    // There are promoted arguments for the requested function
    return BuiltInFunctionCall::BuiltInFunction::BIF_Undefined;
  } else {
    for (ArgListTy::const_iterator ALIT = FuncPosition->second.begin(),
        ALEND = FuncPosition->second.end(); ALIT != ALEND; ++ALIT) {
      if ((ALIT->Argument == Arg) && (ALIT->Function == ArgFunc)) {
        return ALIT->BIF;
      }
    }
  }

  return BuiltInFunctionCall::BuiltInFunction::BIF_Undefined;
}

/// Adds a promotion entry for the given argument into the map and writes an
/// entry into the metadata list. The \c CallArg represents the argument that
/// was provided to the builtin function call. If none was present it should
/// be \c nullptr.
void ArgPromotionTracker::addPromotedArgument(Argument *Arg,
    BuiltInFunctionCall::BuiltInFunction BIF, Constant *CallArg) {
  if (Arg == nullptr) { return; }
  Function *ArgFunc = Arg->getParent();
  if (ArgFunc == nullptr) { return; }
  if (CallArg->getType() != MDCallArgType) { return; }

  // The argument is already associated with an other promotion. So remove it
  // first from the promotion list.
  if (isPromotedArgument(Arg) == true) { return; }

  struct PromArgEntry PAES;
  PAES.Argument = Arg;
  PAES.Function = Arg->getParent();
  PAES.BIF = BIF;
  PAES.CallArg = CallArg;

  PromotedArgs[PAES.Function].push_back(PAES);
  addPromotedArgEntry(PAES.Argument, PAES.BIF, PAES.CallArg);
}

void ArgPromotionTracker::addPromotedArgument(Argument *Arg,
    BuiltInFunctionCall::BuiltInFunction BIF, long CallArg) {
  ConstantInt *CallArgCI = ConstantInt::get(MDCallArgType, CallArg, true);
  addPromotedArgument(Arg, BIF, CallArgCI);
}

void ArgPromotionTracker::addPromotedArgument(Argument *Arg,
    BuiltInFunctionCall::BuiltInFunction BIF) {
  addPromotedArgument(Arg, BIF, nullptr);
}

/// Removes the given argument from the promotion list and from the metadata list.
void ArgPromotionTracker::forgetPromotedArgument(Argument *Arg) {
  if (Arg == nullptr) { return; }
  Function *ArgFunc = Arg->getParent();

  // Remove the argument from the metadata
  removePromotedArgEntry(Arg);

  if (isPromotedArgument(Arg) == false) { return; }

  const MapTy::iterator ArgMapPos = PromotedArgs.find(ArgFunc);
  if (ArgMapPos != PromotedArgs.end()) {
    ArgListTy::iterator ALIT = ArgMapPos->second.begin(), ALEND = ArgMapPos->second.end();
    for ( ; ALIT != ALEND; ++ALIT) {
      if ((ALIT->Argument == Arg) && (ALIT->Function == ArgFunc)) {
        break;
      }
    }
    if (ALIT != ALEND) {
      ArgMapPos->second.erase(ALIT);
    }
  }
}

/// Removes all promoted arguments of the given function from the promotion list
/// and from the metadata list.
void ArgPromotionTracker::forgetPromotedFunctionArguments(Function *Func) {
  if (Func == nullptr) { return; }

  // Remove all promoted arguments for the given function from metadata
  removePromotedArgEntriesForFunction(Func);
  PromotedArgs.erase(Func);
}

unsigned ArgPromotionTracker::size(void) const {
  return PromotedArgs.size();
}

bool ArgPromotionTracker::empty(void) const {
  return PromotedArgs.empty();
}

//===- Helper functions ---------------------------------------------------===//
unsigned ArgPromotionTracker::fillMap(Module *M) {
  if (M == nullptr) { return 0; }

  // Read the list of promoted arguments from the metadata node
  NamedMDNode *MDPromArgList = M->getNamedMetadata(Loopus::KernelMDStrings::OCLACC_PROMARGLIST);
  if (MDPromArgList == nullptr) {
    // There is no list of promoted args available
    return 0;
  }
  if (MDPromArgList->getNumOperands() == 0) { return 0; }

  // Now check all entries
  unsigned NumFoundEntries = 0;
  for (unsigned i = 0, e = MDPromArgList->getNumOperands(); i < e; ++i) {
    MDNode *MDPromArgEntry = MDPromArgList->getOperand(i);
    if (MDPromArgEntry == nullptr) { continue; }
    // We check for less than four to be prepared for more than one argument to
    // an ID builtin function
    if (MDPromArgEntry->getNumOperands() < 3) { continue; }

    // Will be inserted into the list
    struct PromArgEntry PAES;

    // Extract argument index
    Metadata *MDPAEArg = MDPromArgEntry->getOperand(0).get();
    ValueAsMetadata *MDPAEArgVal = dyn_cast<ValueAsMetadata>(MDPAEArg);
    if (MDPAEArgVal == nullptr) { continue; }
    ConstantInt *ArgIdxCI = dyn_cast<ConstantInt>(MDPAEArgVal->getValue());
    if (ArgIdxCI == nullptr) { continue; }

    // Extract function
    Metadata *MDPAEFunc = MDPromArgEntry->getOperand(1).get();
    ValueAsMetadata *MDPAEFuncVal = dyn_cast<ValueAsMetadata>(MDPAEFunc);
    if (MDPAEFuncVal == nullptr) { continue; }
    PAES.Function = dyn_cast<Function>(MDPAEFuncVal->getValue());
    if (PAES.Function == nullptr) { continue; }
    // Determine proper argument
    PAES.Argument = getArgByIndex(PAES.Function, ArgIdxCI);
    if (PAES.Argument == nullptr) { continue; }

    // Extract BIF
    Metadata *MDPAEBIF = MDPromArgEntry->getOperand(2).get();
    MDString *MDPAEBIFString = dyn_cast<MDString>(MDPAEBIF);
    if (MDPAEBIFString == nullptr) { continue; }
    PAES.BIF = resolveStringToBIF(MDPAEBIFString->getString());

    if (MDPromArgEntry->getNumOperands() >= 4) {
      // An argument exists so extract it...
      Metadata *MDPAECallArg = MDPromArgEntry->getOperand(3).get();
      ConstantAsMetadata *MDPAECallArgConst = dyn_cast<ConstantAsMetadata>(MDPAECallArg);
      if (MDPAECallArgConst == nullptr) { continue; }
      PAES.CallArg = MDPAECallArgConst->getValue();
    }

    // If we are here, everything is okay
    PromotedArgs[PAES.Function].push_back(PAES);
    ++NumFoundEntries;
  }
  return NumFoundEntries;
}

//===- Implementation of LLVM pass ----------------------------------------===//
INITIALIZE_PASS_BEGIN(ArgPromotionTracker, "loopus-apt", "Track promoted parameters", false, true)
INITIALIZE_PASS_END(ArgPromotionTracker, "loopus-apt", "Track promoted parameters", false, true)

char ArgPromotionTracker::ID = 0;

namespace llvm {
  Pass* createArgPromotionTrackerPass() {
    return new ArgPromotionTracker();
  }
}

ArgPromotionTracker::ArgPromotionTracker(void)
 : ModulePass(ID), MDCallArgType(nullptr) {
  initializeArgPromotionTrackerPass(*PassRegistry::getPassRegistry());
}

void ArgPromotionTracker::print(raw_ostream &O, const Module *M) const {
  if (PromotedArgs.size() == 0) {
    O << "No promoted arguments found!\n";
    return;
  }

  for (MapTy::const_iterator PAIT = PromotedArgs.cbegin(), PAEND = PromotedArgs.cend();
      PAIT != PAEND; ++PAIT) {
    O << "Promoted Arguments for function: " << PAIT->first->getName() << ":\n";
    for (ArgListTy::const_iterator ALIT = PAIT->second.begin(),
        ALEND = PAIT->second.end(); ALIT != ALEND; ++ALIT) {
      O << " - " << ALIT->Argument->getName() << ": call to BIF " << ALIT->BIF
          << " with arg " << *ALIT->CallArg << "\n";
    }
  }
}

void ArgPromotionTracker::getAnalysisUsage(AnalysisUsage &AU) const {
  AU.setPreservesAll();
}

bool ArgPromotionTracker::runOnModule(Module &M) {
  MDCallArgType = IntegerType::get(getGlobalContext(), 64);
  fillMap(&M);
  return false;
}
