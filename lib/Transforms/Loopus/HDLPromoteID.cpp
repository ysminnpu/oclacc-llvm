//===- PromoteID.cpp - Implementation of PromoteID Pass -------------------===//
//===----------------------------------------------------------------------===//

#define DEBUG_TYPE "hdl-promoteid"

#include "HDLPromoteID.h"

#include "MangledFunctionNames.h"

#include "llvm/ADT/Statistic.h"
#include "llvm/IR/Constant.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Metadata.h"
#include "llvm/IR/Type.h"
#include "llvm/Support/Casting.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/Utils/Cloning.h"

#include <algorithm>

using namespace llvm;

// Support for statistics (using -stats argument for loopus-opt)
STATISTIC(StatsNumDetectedCSs, "Number of detected callsites to builtin functions");
STATISTIC(StatsNumDetectedCSsOthers, "Number of detected callsites to other functions");
STATISTIC(StatsNumCSsReplaced, "Number of replaced callsites/values to builtin functions");
STATISTIC(StatsNumCSsErased, "Number of erased callsites to builtin functions");
STATISTIC(StatsNumKFsRepMod, "Number of replaced/modified kernel functions");
STATISTIC(StatsNumParamsCreated, "Number of newly created parameters for promotion");
STATISTIC(StatsNumBIFsErased, "Number of erased builtin function declarations");

// Add command line parameter for removing unneeded builtin functions
cl::opt<bool> KeepUBIFDeclarations("keepbifdecls", cl::desc("Keep declarations for builtin functions that are not in use after the ID promotion."), cl::Optional, cl::init(false));

/// \brief Determines if the given callsite is a call to an builtin function.
/// \param CI A pointer to the \c CallInst to inspect.
/// \param BIFC A pointer to a BuiltInFunctionCall object to store the call in.
///
/// Determines if the call pointed to by \c CI is a call to an OpenCL ID-related
/// built in function. If so the function stores the appropriate information in
/// the object pointed to by \c BIFC if it is not null and returns \c true.
/// If the callsite is calls some other function \c false is returned and
/// \c BIFC is not modified.
bool HDLPromoteID::isBuiltInFunctionCall(const CallInst *CI,
                                            BuiltInFunctionCall *BIFC) const {
  if (CI == 0) {
    return false;
  }
  if (CI->getCalledFunction() == nullptr) {
    return false;
  }
  StringRef FName = CI->getCalledFunction()->getName();
  auto possibleBIF = BuiltInNameMap.find(FName);

  if (possibleBIF != BuiltInNameMap.end()) {
    // The callsite directs to a builtin
    auto BIFTemplate = possibleBIF->second;

    if (BIFTemplate.second == true) {
      // Calling a builtin function that uses arguments
      assert((CI->getNumArgOperands() == 1) &&
          "Calls to this builtin must always have one argument!");
      if (CI->getNumArgOperands() < 1) { return false; }
      // Evaluate the argument
      ConstantInt *ConstArg = dyn_cast<ConstantInt>(CI->getArgOperand(0));
      if (ConstArg == 0) {
        DEBUG(dbgs() << "Calling builtins with non-const arguments is not "
            << "supported!\n");
        return false;
      }
      unsigned ConstArgVal = ConstArg->getZExtValue();
      // Now adapt the function call object
      if (BIFC != 0) {
        BIFC->CallTarget = BIFTemplate.first;
        BIFC->hasArg = true;
        BIFC->ArgValue = ConstArgVal;
        BIFC->ArgType = CI->getCalledFunction()->getReturnType();
      }
      return true;
    } else {
      // Calling a builtin function that does not use arguments
      assert((CI->getNumArgOperands() == 0) &&
          "Calls to this builtin must not have any arguments!");
      // Now adapt the function call object
      if (BIFC != 0) {
        BIFC->CallTarget = BIFTemplate.first;
        BIFC->hasArg = false;
        BIFC->ArgType = CI->getCalledFunction()->getReturnType();
      }
      return true;
    }
  } else {
    return false;
  }
}

/// \brief Determines all used OpenCL builtin functions.
/// \param F The function to check.
/// \param UsedBuiltIns A vector in which all detected used OpenCL builtin
///                     functions are placed in.
/// \return The number of found used builtin functions.
///
/// Inspects the function body and determines function calls to OpenCL builtin
/// functions. If such a function call is found the according code is placed in
/// \c UsedBuiltIns. The number of those found used builtins is returned.
/// Currently just llvm call instructions and the OpenCL builtin functions for
/// dealing with IDs (have a look at doInitialization) are supported.
unsigned HDLPromoteID::findUsedBuiltIns(Function *F,
    SmallVectorImpl<BuiltInFunctionCall> &UsedBuiltIns) const {
  if (F == nullptr) {
    return 0;
  }
  if (F->isDeclaration() == true) {
    // As declarations do not have a body...
    return 0;
  }

  unsigned CountUsedBuiltIns = 0;
  // We could have used Visitor patterm but as we are just looking for call
  // instructions iterating over all instructions by hand seems okay...
  for (inst_iterator IT = inst_begin(F), END = inst_end(F); IT != END; ++IT) {
    CallInst *CI = dyn_cast<CallInst>(&*IT);
    if (CI != nullptr) {
      BuiltInFunctionCall BIFC;
      if (isBuiltInFunctionCall(CI, &BIFC) == true) {
        DEBUG(dbgs() << "Function: " << F->getName() << ": Callsite to builtin "
            " function \"" << CI->getCalledFunction()->getName() << "\" found.\n");
        ++StatsNumDetectedCSs;

        // By replacing the calls to get_local_size already here we avoid having
        // them in the call-list in the later steps and do have to handle them
        // later on in some special ways
        SmallVectorImpl<BuiltInFunctionCall>::iterator
          first = UsedBuiltIns.begin(), last = UsedBuiltIns.end();
        if (std::find(first, last, BIFC) == last) {
          // That function call did not yet occur
          UsedBuiltIns.push_back(BIFC);
          ++CountUsedBuiltIns;
        }
      } else {
        // There might be call sites to other builtin functions such as sin,
        // cos,... So the llvm_unreachable is not the best idea...
        // llvm_unreachable("Forbidden: callsite to non-builtin function!");
        ++StatsNumDetectedCSsOthers;
      }
    }
  } // End of instruction loop

  return CountUsedBuiltIns;
}

/// \brief Clones the function and adds appropriate parameters to it.
/// \param F The function to clone.
/// \param UsedBuiltIns A list of I- related OpenCL builtin functions that F uses.
/// \return A pointer to the new function or \c 0 in case of an error.
///
/// Clones the given function and appends additional parameters to the cloned
/// functions parameter list: for each used ID-related OpenCL builtin function
/// one new parameter will be added. Then all those function calls are removed
/// from the body and replaced by the appropriate parameter.
Function* HDLPromoteID::createPromotedFunction(const Function &F,
                          SmallVectorImpl<BuiltInFunctionCall> &UsedBuiltIns) {
  // Adding parameter to functions is not that easy. So we do a small hack:
  // 1. Create parameter list for the new function:
  //   a. Get all parameters of the old function
  //   b. Add all new parameters
  // 2. Create a new function with the same name and type.
  // 3. Use CloneFunctionInto to clone old function into the new one
  // 4. Iterate over instructions and search for calls to OpenCL builtin
  //    functions and replace them by the proper parameter.

  // Step 1a: Collect parameter types from old function
  std::vector<Type*> paramtypes;
  for (const Argument &A : F.args()) {
    paramtypes.push_back(A.getType());
  }
  // Step 1b: Add new parameter types
  for (SmallVectorImpl<BuiltInFunctionCall>::iterator UBII = UsedBuiltIns.begin(),
      UBIEND = UsedBuiltIns.end(); UBII != UBIEND; ++UBII) {
    Type *AAT = UBII->ArgType;
    if (AAT != 0) {
      paramtypes.push_back(AAT);
      ++StatsNumParamsCreated;
    } else {
      DEBUG(dbgs() << "Could not create type for new parameter!\n");
      return 0;
    }
  }

  // Step 2: Create function type for new function
  FunctionType *NewFTy = FunctionType::get(
      F.getFunctionType()->getReturnType(), paramtypes,
      F.getFunctionType()->isVarArg());
  if (NewFTy == 0) {
    DEBUG(dbgs() << "Could not get function type (returned null)!\n");
    return 0;
  }
  Function *NewF = Function::Create(NewFTy, F.getLinkage(), F.getName());
  if (NewF == 0) {
    DEBUG(dbgs() << "Could not create new function (returned null)!\n");
    return 0;
  }

  // Step 3: Use CloneFunctionInto to copy function body.
  // Therefore create a map that maps the arguments from the old function to
  // those of the new one.
  ValueToValueMapTy VMap;
  Function::arg_iterator NPI = NewF->arg_begin();
  for (Function::const_arg_iterator OPI = F.arg_begin(), OPEND = F.arg_end();
      OPI != OPEND; ++OPI) {
    NPI->setName(OPI->getName());
    VMap[&*OPI] = &*NPI;
    ++NPI;
  }
  // Now NPI points to the first parameter we inserted as builtin replacement
  unsigned NPIndex = 0;
  for (Function::arg_iterator NPEND = NewF->arg_end(); NPI != NPEND; ++NPI) {
    // Create nice parameter names
    BuiltInFunctionCall &CurrentBIFC = UsedBuiltIns[NPIndex];
    const Loopus::MangledFunctionNames &FNames = Loopus::MangledFunctionNames::getInstance();

    auto BIPBasenameIT = BuiltInFunctionNamesMap.find(CurrentBIFC.CallTarget);
    if (BIPBasenameIT == BuiltInFunctionNamesMap.end()) {
      NPI->setName("promotedparam." + Twine(NPIndex));
    } else {
      std::string ParamBasenameStr = FNames.unmangleName(BIPBasenameIT->second);
      if (CurrentBIFC.hasArg == true) {
        Twine PNameBaseU(ParamBasenameStr + Twine("_"));
        Twine Paramname(PNameBaseU + Twine(CurrentBIFC.ArgValue));
        NPI->setName(Paramname);
      } else {
        Twine PNameBaseD(ParamBasenameStr + Twine("."));
        Twine Paramname(PNameBaseD + Twine(NPIndex));
        NPI->setName(Paramname);
      }
    }
    // Store a pointer to that argument in the specific builtin call object
    CurrentBIFC.PromotedArg = &*NPI;
    ++NPIndex;
  }

  // Step 4: Now let CloneFunctionInto do the main work of cloning.
  SmallVector<ReturnInst*, 8> Returns;
  CloneFunctionInto(NewF, &F, VMap, false, Returns);

  // Now replace all callsites to builtins with their corresponding parameter
  unsigned replacedCIs = 0;
  std::vector<Instruction*> BuiltInCallSites;
  for (inst_iterator IT = inst_begin(NewF), END = inst_end(NewF); IT != END; ++IT) {
    CallInst *CI = dyn_cast<CallInst>(&*IT);
    if (CI != 0) {
      BuiltInFunctionCall BIFCTmp;
      if (isBuiltInFunctionCall(CI, &BIFCTmp) == true) {
        SmallVectorImpl<BuiltInFunctionCall>::iterator
          first = UsedBuiltIns.begin(), last = UsedBuiltIns.end(), index = UsedBuiltIns.end();
        index = std::find(first, last, BIFCTmp);
        if (index == last) {
          DEBUG(dbgs() << "Function: " << F.getName() << ": did not process "
              << "callsite to builtin " << CI->getCalledFunction()->getName()
              << "! Could not replace by parameter!\n");
        } else {
          Argument *CIParam = index->PromotedArg;
          CI->replaceAllUsesWith(CIParam);
          BuiltInCallSites.push_back(CI);
          ++replacedCIs;
          ++StatsNumCSsReplaced;
        }
      }
    }
  }
  DEBUG(dbgs() << "Function: " << NewF->getName() << ": replaced " << replacedCIs
      << " call sites!\n");

  unsigned erasedCIs = 0;
  for (unsigned ECIIndex = 0; ECIIndex < BuiltInCallSites.size(); ++ECIIndex) {
    BuiltInCallSites[ECIIndex]->eraseFromParent();
    BuiltInCallSites[ECIIndex] = 0;
    ++erasedCIs;
    ++StatsNumCSsErased;
  }
  DEBUG(dbgs() << "Function: " << NewF->getName() << ": erased " << erasedCIs
      << " call sites!\n");

  return NewF;
}

//===- Implementation of LLVM pass ----------------------------------------===//
INITIALIZE_PASS_BEGIN(HDLPromoteID, "loopus-promid", "Promote IDs to parameters",  false, false)
INITIALIZE_PASS_DEPENDENCY(ArgPromotionTracker);
INITIALIZE_PASS_DEPENDENCY(OpenCLMDKernels)
INITIALIZE_PASS_END(HDLPromoteID, "loopus-promid", "Promote IDs to parameters",  false, false)

char HDLPromoteID::ID = 0;

namespace llvm {
  Pass* createHDLPromoteIDPass() {
    return new HDLPromoteID();
  }
}

HDLPromoteID::HDLPromoteID()
 : ModulePass(ID), APT(nullptr), OCLKernels(nullptr) {
  initializeHDLPromoteIDPass(*PassRegistry::getPassRegistry());
}

void HDLPromoteID::getAnalysisUsage(AnalysisUsage &AU) const {
  AU.addRequired<ArgPromotionTracker>();
  AU.addRequired<OpenCLMDKernels>();
}

bool HDLPromoteID::runOnModule(Module &M) {
  // We do not have to care about declarations:
  // It seems as if declarations in LLVM IR declare functions that do not
  // provide a body in the IR file. So we cannot determine if the function uses
  // a builtin function. For that we do not modify them and the declaration
  // must not be updated.

  // Now collect all kernel functions
  // OpenCLMDKernels &OCLKernels = getAnalysis<OpenCLMDKernels>();
  APT = &getAnalysis<ArgPromotionTracker>();
  OCLKernels = &getAnalysis<OpenCLMDKernels>();
  std::vector<Function*> KernelFs;
  OCLKernels->getKernelFunctions(KernelFs);

  // Next step is to collect used builtin functions for all kernel functions
  std::map<llvm::Function*, llvm::SmallVector<BuiltInFunctionCall, 10> > FBuiltInsMap;
  for (Function *KF : KernelFs) {
    if (KF->isDeclaration() == true) { continue; }
    SmallVector<BuiltInFunctionCall, 10> KFUsedBuiltIns;
    if (findUsedBuiltIns(KF, KFUsedBuiltIns) > 0) {
      FBuiltInsMap[KF] = KFUsedBuiltIns;
    } else {
      DEBUG(dbgs() << "Function " << KF->getName() << ": Does not use ID-"
          << "related builtins.\n");
    }
  }

  // Now create new parameters for all used builtins and replace the callsites
  // To store the mapping between the old and the new functions
  llvm::ValueToValueMapTy FOldNewMapping;
  // Just do the work for all functions that use replaceable builtins
  for (std::map<Function*, SmallVector<BuiltInFunctionCall, 10> >::iterator
      KFI = FBuiltInsMap.begin(), KFEND = FBuiltInsMap.end(); KFI != KFEND; ++KFI) {
    Function *NewF = createPromotedFunction(*(KFI->first), KFI->second);
    if (NewF != 0) {
      FOldNewMapping[KFI->first] = NewF;
    } else {
      DEBUG(dbgs() << "Could not create promoted function for kernel " <<
          "function " << KFI->first->getName() << "!\n");
    }
  }

  // Collect the metadata nodes for creating the oclacc.workitem and
  // oclacc.single metadata nodes.
  SmallVector<MDNode*, 10> MDNsWorkitemFs, MDNsSingleFs;

  // We need to update the opencl.kernels metadata node (and all nodes
  // that are referenced from there)...
  for (Function *KF : KernelFs) {
    if (KF->isDeclaration() == true) { continue; }

    bool replaceFNeeded = true;
    bool replacedF = false;
    bool replaceMDNeeded = true;
    bool replacedMD = false;

    // MDFN points to a metadata node representing one kernel function
    MDNode *MDFN = OCLKernels->getMDNodeForFunction(KF);
    if (MDFN == 0) { continue; }
    const unsigned NumMDKernelFOps = MDFN->getNumOperands();
    if (NumMDKernelFOps == 0) { continue; }

    // Just for security: not really needed!
    // There must be at least one entry for the function pointer itself
    // Replace that function pointer with a pointer to the new function
    const Function *MDF = mdconst::dyn_extract<Function>(MDFN->getOperand(0));
    if (MDF == 0) { continue; }
    // This is not the function we are looking for. Keep searching...
    if (MDF != KF) { continue; }

    // So MDF/KF is the function we want to replace
    ValueToValueMapTy::iterator NewKFIT = FOldNewMapping.find(KF);
    // Check if there is a proper new function
    if (NewKFIT == FOldNewMapping.end()) {
      replaceFNeeded = false;
      // Add the metadata node corresponding to the current kernel to the list
      // of single kernel functions
      MDNsSingleFs.push_back(MDFN);
      DEBUG(dbgs() << "Function: " << KF->getName() << ": must not be replaced!\n");
      continue;
    }

    // The function seems to be replaced later on meaning that it used builtin
    // functions. So add it to the workitem list
    MDNsWorkitemFs.push_back(MDFN);
    Function *NewKF = dyn_cast<Function>(NewKFIT->second);
    if (NewKF == 0) {
      DEBUG(dbgs() << "Function: " << KF->getName() << ": NewKF was null!\n");
      continue;
    }
    // We cannot use replaceAllUsesWith as the new function has a different
    // type than the old one
    ValueAsMetadata *MDNewKF = ValueAsMetadata::get(NewKF);
    MDFN->replaceOperandWith(0, MDNewKF);
    replacedF = true;

    // Get the list of added parameters
    auto KFBFsIT = FBuiltInsMap.find(KF);
    if (KFBFsIT == FBuiltInsMap.end()) {
      DEBUG(dbgs() << "Function: " << KF->getName() << ": metadata must "
          << "not be replaced!\n");
      replaceMDNeeded = false;
      continue;
    }
    const SmallVectorImpl<BuiltInFunctionCall> &BIFCs = KFBFsIT->second;
    replaceMDNeeded = (BIFCs.size() > 0);

    // Now adapt the remaining metadata nodes (this gets really annoying...)
    // The loop begins with 1 intentionally as the MDOperand at index 0 is
    // a pointer to the kernel function itself that we already replaced above
    for (unsigned MDOpIndex = 1; MDOpIndex < NumMDKernelFOps; ++MDOpIndex) {
      if (BIFCs.size() == 0) {
        // We do not need to do adapt any nodes as there are no used builtin
        // function calls
        break;
      }

      // MDKernelParamInfoND points to a meta object representing one single
      // information for each of the parameters
      MDNode *MDKernelParamInfoND =
        dyn_cast<MDNode>(MDFN->getOperand(MDOpIndex).get());
      if (MDKernelParamInfoND == 0) { continue; }
      const unsigned NumMDKernelInfoOps = MDKernelParamInfoND->getNumOperands();
      if (NumMDKernelInfoOps == 0) { continue; }
      // Now the first operand of MDKernelParamInfoND should be a string
      // indicating which type of information is stored in the current node
      const MDString *MDKernelParamInfoDescription =
        dyn_cast<MDString>(MDKernelParamInfoND->getOperand(0).get());
      if (MDKernelParamInfoDescription == 0) { continue; }
      const StringRef KParIDesc = MDKernelParamInfoDescription->getString();

      LLVMContext &GlobalC = getGlobalContext();
      LLVMContext &LocalC = MDKernelParamInfoND->getContext();

      // Metadata for our new parameters
      SmallVector<Metadata*, 10> MDBIFCs;
      // Now fill it
      for (const BuiltInFunctionCall &BIFC : BIFCs) {
        if (KParIDesc.equals(Loopus::KernelMDStrings::SPIR_ARG_ADDR_SPACES) == true) {
          // Add address spaces for new parameters
          IntegerType *IntTyAddrSpace = IntegerType::get(GlobalC, 32);
          ConstantInt *CIKernelParamAddrSpace = ConstantInt::get(
              IntTyAddrSpace, Loopus::OpenCLAddressSpaces::ADDRSPACE_PRIVATE);
          ConstantAsMetadata *MDCIAddrSpace =
            ConstantAsMetadata::get(CIKernelParamAddrSpace);
          if (MDCIAddrSpace == 0) { continue; }
          MDBIFCs.push_back(MDCIAddrSpace);

        } else if (KParIDesc.equals(Loopus::KernelMDStrings::SPIR_ARG_ACCESS_QUAL) == true) {
          // The SPIR compiler seems to add "none" for all parameters
          MDString *MDAccessQual = MDString::get(LocalC, "none");
          if (MDAccessQual == 0) { continue; }
          MDBIFCs.push_back(MDAccessQual);

        } else if (KParIDesc.equals(Loopus::KernelMDStrings::SPIR_ARG_OPTIONAL) == true) {
          DEBUG(dbgs() << "Unsupported parameter information \""
              << Loopus::KernelMDStrings::SPIR_ARG_OPTIONAL << "\" found!\n");

        } else if (KParIDesc.equals(Loopus::KernelMDStrings::SPIR_ARG_TYPE) == true) {
          // Experiments with SPIR-clang showed the following results
          if (BIFC.CallTarget == BuiltInFunctionCall::BuiltInFunction::BIF_GetWorkDim) {
            MDString *MDParamType = MDString::get(LocalC, "uint");
            if (MDParamType == 0) { continue; }
            MDBIFCs.push_back(MDParamType);
          } else {
            MDString *MDParamType = MDString::get(LocalC, "size_t");
            if (MDParamType == 0) { continue; }
            MDBIFCs.push_back(MDParamType);
          }

        } else if (KParIDesc.equals(Loopus::KernelMDStrings::SPIR_ARG_BASETYPE) == true) {
          // Experiments with SPIR-clang show that for kernel_arg_base_type
          // both size_t and uint are mapped to the string "uint"
          MDString *MDParamBaseType = MDString::get(LocalC, "uint");
          if (MDParamBaseType == 0) { continue; }
          MDBIFCs.push_back(MDParamBaseType);

        } else if (KParIDesc.equals(Loopus::KernelMDStrings::SPIR_ARG_TYPE_QUAL) == true) {
          MDString *MDParamTypeQual = MDString::get(LocalC, "const");
          if (MDParamTypeQual == 0) { continue; }
          MDBIFCs.push_back(MDParamTypeQual);

        } else if (KParIDesc.equals(Loopus::KernelMDStrings::SPIR_ARG_NAME) == true) {
          MDString *MDParamName = MDString::get(LocalC, BIFC.PromotedArg->getName());
          if (MDParamName == 0) { continue; }
          MDBIFCs.push_back(MDParamName);

        } else {
          DEBUG(dbgs() << "Unknown information node \"" << KParIDesc
              << "\" found!\n");
        }
      }

      if (MDBIFCs.size() > 0) {
        DEBUG(dbgs() << "Function " << KF->getName() << ": Need to update "
            << "metadata parameter information \"" << KParIDesc << "\" as "
            << "builtins were used!\n");
        // New metadata was created so add it...
        MDNode *MDBIParamsND = MDNode::get(LocalC, MDBIFCs);
        if (MDBIParamsND == 0) { continue; }
        MDNode *MDNewKernelParamInfoND = MDNode::concatenate(
            MDKernelParamInfoND, MDBIParamsND);
        if (MDNewKernelParamInfoND == 0) { continue; }
        // Replace the old MDNode with the new one
        MDFN->replaceOperandWith(MDOpIndex, MDNewKernelParamInfoND);
        replacedMD = true;
      }
    } // End-of-Loop over the metadata nodes giving parameter information

    if (replaceFNeeded != replaceMDNeeded) {
      DEBUG(dbgs() << "Function " << KF->getName() << ": Mismatch between "
          << "replaceFNeeded and replaceMDNeeded!\n");
    }
    if (replaceFNeeded == true) {
      if (replacedF == false) {
        DEBUG(dbgs() << "Function " << KF->getName() << ": "
            << "Did not replace function in "
            << Loopus::KernelMDStrings::OPENCL_KERNELS_ROOT << " metadata node! "
            << "Metadata might be inconsistent!\n");
      }
      if ((replaceMDNeeded == true) && (replacedMD == false)) {
        DEBUG(dbgs() << "Function " << KF->getName() << ": "
            << "Did not replace parameter information metadata objects in "
            << Loopus::KernelMDStrings::OPENCL_KERNELS_ROOT << " "
            << "metadata node! Metadata might be inconsistent!\n");
      }
    }

  } // End-of-Loop over all existing kernel functions in the module

  // Now create the oclacc.workitem and oclacc.single metadata nodes
  // Check if oclacc.workitem node already exists
  NamedMDNode *OclaccWorkItemsNMDN =
      M.getNamedMetadata(Loopus::KernelMDStrings::OCLACC_ISWORKITEM);
  if (OclaccWorkItemsNMDN != 0) {
    DEBUG(dbgs() << "A metadata node named \""
        << Loopus::KernelMDStrings::OCLACC_ISWORKITEM << "\" already "
        << "exists! Now erasing and recreating node.\n");
    OclaccWorkItemsNMDN->eraseFromParent();
    OclaccWorkItemsNMDN = 0;
  }
  // Now the node should not exists any more (if it ever did)
  OclaccWorkItemsNMDN =
      M.getOrInsertNamedMetadata(Loopus::KernelMDStrings::OCLACC_ISWORKITEM);
  for (MDNode *MDN : MDNsWorkitemFs) {
    OclaccWorkItemsNMDN->addOperand(MDN);
  }

  // Now the same for oclacc.single: check if it already exists
  NamedMDNode *OclaccSingleNMDN =
      M.getNamedMetadata(Loopus::KernelMDStrings::OCLACC_ISSINGLE);
  if (OclaccSingleNMDN != 0) {
    DEBUG(dbgs() << "A metadata node named \""
        << Loopus::KernelMDStrings::OCLACC_ISSINGLE << "\" already "
        << "exists! Now erasing and recreating node.\n");
    OclaccSingleNMDN->eraseFromParent();
    OclaccSingleNMDN = 0;
  }
  // Now the node should not exists any more (if it ever did)
  OclaccSingleNMDN =
      M.getOrInsertNamedMetadata(Loopus::KernelMDStrings::OCLACC_ISSINGLE);
  for (MDNode *MDN : MDNsSingleFs) {
    OclaccSingleNMDN->addOperand(MDN);
  }

  // Now replace the old functions by the new ones
  for (ValueToValueMapTy::iterator FI = FOldNewMapping.begin(),
      FEND = FOldNewMapping.end(); FI != FEND; ++FI) {
    Function *OldF = const_cast<Function*>(dyn_cast<Function>(FI->first));
    Function *NewF = dyn_cast<Function>(FI->second);

    // Get the list of used builtins
    SmallVectorImpl<BuiltInFunctionCall> &UsedBIFCs = FBuiltInsMap[OldF];
    // Delete old function. It seems as if eraseFromParent also removes the
    // entry from FOldNewMapping.
    APT->forgetPromotedFunctionArguments(OldF);
    OldF->eraseFromParent();
    // Insert new function
    M.getFunctionList().push_back(NewF);

    // Now update the list of promoted arguments
    for (BuiltInFunctionCall CurrentBIFC : UsedBIFCs) {
      if (CurrentBIFC.hasArg == true) {
        APT->addPromotedArgument(CurrentBIFC.PromotedArg, CurrentBIFC.CallTarget, CurrentBIFC.ArgValue);
      } else {
        APT->addPromotedArgument(CurrentBIFC.PromotedArg, CurrentBIFC.CallTarget);
      }
    }
    ++StatsNumKFsRepMod;
  }

  // Now remove any unused builtin function declarations if wished...
  if (KeepUBIFDeclarations == false) {
    DEBUG(dbgs() << "Removing unused builtin function declarations:\n");
    std::vector<Function*> UnusedBIFs;
    for (Function &CF : M.functions()) {
      DEBUG(dbgs() << "Testing function " << CF.getName());
      auto FIT = BuiltInNameMap.find(CF.getName());
      if (FIT == BuiltInNameMap.end()) {
        DEBUG(dbgs() << ": [not marked]\n");
        continue;
      }

      // CF is a builtin function
      if (CF.user_empty() == true) {
        DEBUG(dbgs() << ": [unused] [marked]\n");
        UnusedBIFs.push_back(&CF);
      } else {
        DEBUG(dbgs() << ": [in use]\n");
      }
    }
    for (Function *EF : UnusedBIFs) {
      DEBUG(dbgs() << "Erasing function " << EF->getName() << "...");
      EF->eraseFromParent();
      DEBUG(dbgs() << " done\n");
      ++StatsNumBIFsErased;
    }
  }

  // We do not need to adapt any callsites as there should not be anymore left:
  // - kernel     > kernel      : forbidden
  // - kernel     > non-kernel  : inlined
  // - non-kernel > kernel      : forbidden
  // - non-kernel > non-kernel  : inlined

  // Assuming this function always modifies the code
  return true;
}

/// \brief Initialize pass by inserting all builtin functions that should
/// \brief be replaced by an additional parameter into a map.
bool HDLPromoteID::doInitialization(Module &M) {
  // Insert all builtin functions that should be detected and replaced by an
  // additional parameter into the map.
  // The mangled name is used as key. The value to each key is a pair of
  // an enum type and a bool: the enum represents the called builtin function
  // (to avoid string comparisons) and the bool indicates if the associated
  // builtin function expects one argument (and only one).
  BuiltInFunctionNamesMap[BuiltInFunctionCall::BuiltInFunction::BIF_Undefined] =
    "bi_undef_builtin";
  // get_work_dim
  BuiltInNameMap["_Z12get_work_dim"] =
    std::make_pair<BuiltInFunctionCall::BuiltInFunction, bool>
    (BuiltInFunctionCall::BuiltInFunction::BIF_GetWorkDim, false);
  BuiltInFunctionNamesMap[BuiltInFunctionCall::BuiltInFunction::BIF_GetWorkDim] =
    "_Z12get_work_dim";
  // get_global_size
  BuiltInNameMap["_Z15get_global_sizej"] =
    std::make_pair<BuiltInFunctionCall::BuiltInFunction, bool>
    (BuiltInFunctionCall::BuiltInFunction::BIF_GetGlobalSize, true);
  BuiltInFunctionNamesMap[BuiltInFunctionCall::BuiltInFunction::BIF_GetGlobalSize] =
    "_Z15get_global_sizej";
  // get_global_id
  BuiltInNameMap["_Z13get_global_idj"] =
    std::make_pair<BuiltInFunctionCall::BuiltInFunction, bool>
    (BuiltInFunctionCall::BuiltInFunction::BIF_GetGlobalID, true);
  BuiltInFunctionNamesMap[BuiltInFunctionCall::BuiltInFunction::BIF_GetGlobalID] =
    "_Z13get_global_idj";
  // get_local_size
  BuiltInNameMap["_Z14get_local_sizej"] =
    std::make_pair<BuiltInFunctionCall::BuiltInFunction, bool>
    (BuiltInFunctionCall::BuiltInFunction::BIF_GetLocalSize, true);
  BuiltInFunctionNamesMap[BuiltInFunctionCall::BuiltInFunction::BIF_GetLocalSize] =
    "_Z14get_local_sizej";
  // get_enqued_local_size
  BuiltInNameMap["_Z23get_enqueued_local_sizej"] =
    std::make_pair<BuiltInFunctionCall::BuiltInFunction, bool>
    (BuiltInFunctionCall::BuiltInFunction::BIF_GetEnqLocalSize, true);
  BuiltInFunctionNamesMap[BuiltInFunctionCall::BuiltInFunction::BIF_GetEnqLocalSize] =
    "_Z23get_enqueued_local_sizej";
  // get_local_id
  BuiltInNameMap["_Z12get_local_idj"] =
    std::make_pair<BuiltInFunctionCall::BuiltInFunction, bool>
    (BuiltInFunctionCall::BuiltInFunction::BIF_GetLocalID, true);
  BuiltInFunctionNamesMap[BuiltInFunctionCall::BuiltInFunction::BIF_GetLocalID] =
    "_Z12get_local_idj";
  // get_num_groups
  BuiltInNameMap["_Z14get_num_groupsj"] =
    std::make_pair<BuiltInFunctionCall::BuiltInFunction, bool>
    (BuiltInFunctionCall::BuiltInFunction::BIF_GetNumGroups, true);
  BuiltInFunctionNamesMap[BuiltInFunctionCall::BuiltInFunction::BIF_GetNumGroups] =
    "_Z14get_num_groupsj";
  // get_group_id
  BuiltInNameMap["_Z12get_group_idj"] =
    std::make_pair<BuiltInFunctionCall::BuiltInFunction, bool>
    (BuiltInFunctionCall::BuiltInFunction::BIF_GetGroupID, true);
  BuiltInFunctionNamesMap[BuiltInFunctionCall::BuiltInFunction::BIF_GetGroupID] =
    "_Z12get_group_idj";
  // get_global_offset
  BuiltInNameMap["_Z17get_global_offsetj"] =
    std::make_pair<BuiltInFunctionCall::BuiltInFunction, bool>
    (BuiltInFunctionCall::BuiltInFunction::BIF_GetGlobalOffset, true);
  BuiltInFunctionNamesMap[BuiltInFunctionCall::BuiltInFunction::BIF_GetGlobalOffset] =
    "_Z17get_global_offsetj";
  // get_global_linear_id
  BuiltInNameMap["_Z20get_global_linear_id"] =
    std::make_pair<BuiltInFunctionCall::BuiltInFunction, bool>
    (BuiltInFunctionCall::BuiltInFunction::BIF_GetGlobLinearID, false);
  BuiltInFunctionNamesMap[BuiltInFunctionCall::BuiltInFunction::BIF_GetGlobLinearID] =
    "_Z20get_global_linear_id";
  // get_local_linear_id
  BuiltInNameMap["_Z19get_local_linear_id"] =
    std::make_pair<BuiltInFunctionCall::BuiltInFunction, bool>
    (BuiltInFunctionCall::BuiltInFunction::BIF_GetLocLinearID, false);
  BuiltInFunctionNamesMap[BuiltInFunctionCall::BuiltInFunction::BIF_GetLocLinearID] =
    "_Z19get_local_linear_id";
  return true;
}

