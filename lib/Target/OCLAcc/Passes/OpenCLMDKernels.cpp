//===- OpenCLMDKernels.cpp - Implementation of OpenCLMDKernels pass -------===//
//===----------------------------------------------------------------------===//

#define DEBUG_TYPE "openclmdkernels"

#include "OpenCLMDKernels.h"

#include "LoopusUtils.h"

#include "llvm/ADT/Statistic.h"
#include "llvm/IR/Function.h"
#include "llvm/Support/Debug.h"

#include <set>

using namespace llvm;

STATISTIC(StatsNumInspectedMDs, "Number of inspected metadata subnodes of opencl.kernels");
STATISTIC(StatsNumDetectedKernelFs, "Number of detected kernel functions in metadata nodes");

//===- Implementation of LLVM pass ----------------------------------------===//
INITIALIZE_PASS_BEGIN(OpenCLMDKernels, "loopus-oclmdkernels", "Detect OpenCL kernel functions",  false, true)
INITIALIZE_PASS_END(OpenCLMDKernels, "loopus-oclmdkernels", "Detect OpenCL kernel functions",  false, true)

char OpenCLMDKernels::ID = 0;

namespace llvm {
  Pass* createOpenCLMDKernelsPass() {
    return new OpenCLMDKernels();
  }
}

OpenCLMDKernels::OpenCLMDKernels(void)
 : ModulePass(ID), foundMDNOpenCLKernels(false), foundMDNOclaccWorkitems(false) {
  initializeOpenCLMDKernelsPass(*PassRegistry::getPassRegistry());
}

void OpenCLMDKernels::getAnalysisUsage(AnalysisUsage &AU) const {
  // We are an analysis pass and do not modify anything...
  AU.setPreservesAll();
}

void OpenCLMDKernels::print(raw_ostream &O, const Module *M) const {
  if (foundMDNOpenCLKernels == false) {
    O << "Did not find metadata node \"opencl.kernels\"! Could not determine "
      << "list of available kernel functions!\n";
    return;
  }

  for (auto KFInfoIT = KernelInfo.begin(), KFInfoEND = KernelInfo.end();
      KFInfoIT != KFInfoEND; ++KFInfoIT) {
    const Function* const KF = KFInfoIT->first;
    const KernelFInfo &KInfo = KFInfoIT->second;

    O << "Found Function: " << KF->getName() << ": ";
    if (KInfo.Flags.isKernelF == true) {
      O << "[kernel] ";
    } else {
      O << "[nokernel] ";
    }
    if (foundMDNOclaccWorkitems == true) {
      if (KInfo.Flags.isWorkItem == true) {
        O << "[workitem] ";
      } else {
        O << "[single] ";
      }
    } else {
      O << "[?] ";
    }
    O << "\n";
  }
}

/// \brief The function tries to extract all kernel functions from the metadata
/// \brief node named opencl.kernels.
///
/// The function searches for the metadata nodes opencl.kernels and tries all
/// functions listed in it. If the operation completed successfully and no
/// malformed nodes were found the function returns \c true. Else \c false will
/// be returned.
bool OpenCLMDKernels::evaluateOpenCLKernelsMD(const Module &M) {
  NamedMDNode *MDKernelsMND =
      M.getNamedMetadata(Loopus::KernelMDStrings::OPENCL_KERNELS_ROOT);
  if (MDKernelsMND == 0) {
    DEBUG(dbgs() << "No metadata node \""
        << Loopus::KernelMDStrings::OPENCL_KERNELS_ROOT << "\" containing "
        << "kernel functions detected! Aborting...\n");
    foundMDNOpenCLKernels = false;
    return false;
  }

  foundMDNOpenCLKernels = true;
  const unsigned NumMDKernelsMNDOps = MDKernelsMND->getNumOperands();
  if (NumMDKernelsMNDOps == 0) {
    // There are no kernels in the metadata node
    DEBUG(dbgs() << "Metadata does not contain any kernel functions!\n");
    KernelInfo.clear();
    return true;
  }

  // Now iterate over all detected kernels and put them into the map
  bool ErrorOccured = false;
  for (unsigned KFIndex = 0; KFIndex < NumMDKernelsMNDOps; ++KFIndex) {
    // MDKernelFND points to a metadata node representing a kernel function
    // with its associated argument information
    MDNode *MDKernelFND = MDKernelsMND->getOperand(KFIndex);
    if (MDKernelFND == 0) {
      DEBUG(dbgs() << "Could not examine kernel function at "
          << Loopus::KernelMDStrings::OPENCL_KERNELS_ROOT
          << "[" << KFIndex << "]. getOperand returned null!\n");
      ErrorOccured = true;
      continue;
    }

    ++StatsNumInspectedMDs;

    // Determine number of available information nodes for current kernel
    const unsigned NumMDKernelFNDOps = MDKernelFND->getNumOperands();
    if (NumMDKernelFNDOps == 0) {
      DEBUG(dbgs() << "Invalid formed metadata node found for kernel function "
          << "at " << Loopus::KernelMDStrings::OPENCL_KERNELS_ROOT << "["
          << KFIndex << "]: node does not contain any "
          << "Function* or information metadata nodes.\n");
      ErrorOccured = true;
      continue;
    }

    // Extract function from subnode. According to the SPIR2.0 specification the
    // First element is always the function signature (or in LLVM IR a function
    // pointer) itself
    Function *KernelF =
      mdconst::dyn_extract<Function>(MDKernelFND->getOperand(0).get());
    if (KernelF == 0) {
      DEBUG(dbgs() << "Metadata node for "
          << Loopus::KernelMDStrings::OPENCL_KERNELS_ROOT << "[" << KFIndex
          << "] does not contain a valid function at subnode 0!\n");
      ErrorOccured = true;
      continue;
    }

    ++StatsNumDetectedKernelFs;

    // Now create new info struct for this function
    KernelFInfoFlags KFIFlags;
    KFIFlags.isKernelF = true;
    KFIFlags.isWorkItem = false;
    KernelFInfo KFInfo(KFIFlags);
    KFInfo.MDN = MDKernelFND;
    KernelInfo[KernelF] = KFInfo;
  }

  return !ErrorOccured;
}

/// \brief Detects the kernel functions that are listed in the oclacc.workitem
/// \brief and oclacc.single metadata nodes and sets the associated flags.
///
/// The function inspects the metadata nodes oclacc.workitem and oclacc.single,
/// extracts the kernel functions from them and sets the proper flags for them.
/// If the operation completed successfully and no malformed nodes were found
/// \c true will be returned. Else the function returns \c false.
/// The function assumes that the \c KernelInfo map is already properly filled.
bool OpenCLMDKernels::evaluateOclaccWorkitems(const Module &M) {
  NamedMDNode *MDKWorkitemsMND =
      M.getNamedMetadata(Loopus::KernelMDStrings::OCLACC_ISWORKITEM);
  NamedMDNode *MDKSingleMND =
      M.getNamedMetadata(Loopus::KernelMDStrings::OCLACC_ISSINGLE);
  if ((MDKWorkitemsMND == 0) && (MDKSingleMND == 0)) {
    DEBUG(dbgs() << "Neither a metadata node named \""
        << Loopus::KernelMDStrings::OCLACC_ISWORKITEM << "\" nor \""
        << Loopus::KernelMDStrings::OCLACC_ISSINGLE << " was found: Aborting\n");
    foundMDNOclaccWorkitems = false;
    return false;
  }
  foundMDNOclaccWorkitems = true;

  // First build list/set containg the workitem kernel functions
  std::set<Function*> KWorkitems;
  const unsigned NumMDKWorkitemsMNDOps = MDKWorkitemsMND->getNumOperands();
  bool ErrorOccured = false;
  for (unsigned KFWIndex = 0; KFWIndex < NumMDKWorkitemsMNDOps; ++KFWIndex) {
    // MDKernelFND points to the same metadata node as the metadata object in
    // the opencl.kernels node. So they can be treated in the exact same manner.
    const MDNode *MDKernelFND = MDKWorkitemsMND->getOperand(KFWIndex);
    if (MDKernelFND == 0) {
      DEBUG(dbgs() << "Could not examine kernel function at "
          << Loopus::KernelMDStrings::OCLACC_ISWORKITEM
          << "[" << KFWIndex << "]. getOperand returned null!\n");
      ErrorOccured = true;
      continue;
    }

    // Determine number of available information nodes for current kernel
    const unsigned NumMDKernelFNDOps = MDKernelFND->getNumOperands();
    if (NumMDKernelFNDOps == 0) {
      DEBUG(dbgs() << "Invalid formed metadata node found for kernel function "
          << "at " << Loopus::KernelMDStrings::OCLACC_ISWORKITEM << "["
          << KFWIndex << "]: node does not contain any Function* or "
          << "information metadata nodes.\n");
      ErrorOccured = true;
      continue;
    }

    // Extract function from subnode. According to the SPIR2.0 specification the
    // First element is always the function signature (or in LLVM IR a function
    // pointer) itself
    Function *KernelF =
      mdconst::dyn_extract<Function>(MDKernelFND->getOperand(0).get());
    if (KernelF == 0) {
      DEBUG(dbgs() << "Metadata node for "
          << Loopus::KernelMDStrings::OCLACC_ISWORKITEM << "[" << KFWIndex
          << "] does not contain a valid function at subnode 0!\n");
      ErrorOccured = true;
      continue;
    }

    // Now insert function into set
    KWorkitems.insert(KernelF);
  }
  if (NumMDKWorkitemsMNDOps != KWorkitems.size()) {
    DEBUG(dbgs() << "Some of the kernel functions listed in "
        << Loopus::KernelMDStrings::OCLACC_ISWORKITEM << " were ignored!\n");
  }

  // Now build the list/set containing the single kernel functions
  std::set<Function*> KSingle;
  const unsigned NumMDKSingleMNDOps = MDKSingleMND->getNumOperands();
  for (unsigned KFSIndex = 0; KFSIndex < NumMDKSingleMNDOps; ++KFSIndex) {
    // MDKernelFND points to the same metadata node as the metadata object in
    // the opencl.kernels node. So they can be treated in the exact same manner.
    const MDNode *MDKernelFND = MDKSingleMND->getOperand(KFSIndex);
    if (MDKernelFND == 0) {
      DEBUG(dbgs() << "Could not examine kernel function at "
          << Loopus::KernelMDStrings::OCLACC_ISSINGLE
          << "[" << KFSIndex << "]. getOperand returned null!\n");
      ErrorOccured = true;
      continue;
    }

    // Determine number of available information nodes for current kernel
    const unsigned NumMDKernelFNDOps = MDKernelFND->getNumOperands();
    if (NumMDKernelFNDOps == 0) {
      DEBUG(dbgs() << "Invalid formed metadata node found for kernel function "
          << "at " << Loopus::KernelMDStrings::OCLACC_ISSINGLE << "["
          << KFSIndex << "]: node does not contain any Function* or "
          << "information metadata nodes.\n");
      ErrorOccured = true;
      continue;
    }

    // Extract function from subnode. According to the SPIR2.0 specification the
    // First element is always the function signature (or in LLVM IR a function
    // pointer) itself
    Function *KernelF =
      mdconst::dyn_extract<Function>(MDKernelFND->getOperand(0).get());
    if (KernelF == 0) {
      DEBUG(dbgs() << "Metadata node for "
          << Loopus::KernelMDStrings::OCLACC_ISSINGLE << "[" << KFSIndex << "] "
          << "does not contain a valid function at subnode 0!\n");
      ErrorOccured = true;
      continue;
    }

    // Now insert function into set
    KSingle.insert(KernelF);
  }
  if (NumMDKSingleMNDOps != KSingle.size()) {
    DEBUG(dbgs() << "Some of the kernel functions listed in "
        << Loopus::KernelMDStrings::OCLACC_ISSINGLE << "were ignored!\n");
  }

  // Make sure that we did not find any functions that are not listed as kernel
  for (Function* KernelF : KWorkitems) {
    auto KernelFIT = KernelInfo.find(KernelF);
    if (KernelFIT == KernelInfo.end()) {
      DEBUG(dbgs() << "Function: " << KernelF->getName() << ": was declared as "
          << "workitem function but is not listed as kernel!\n");
      ErrorOccured = true;
    }
  }
  for (Function* KernelF : KSingle) {
    auto KernelFIT = KernelInfo.find(KernelF);
    if (KernelFIT == KernelInfo.end()) {
      DEBUG(dbgs() << "Function: " << KernelF->getName() << ": was declared as "
          << "single function but is not listed as kernel!\n");
      ErrorOccured = true;
    }
  }

  // Set proper flags for functions and perform some on-the-fly consistency
  // checks
  for (auto KernelFsIT = KernelInfo.begin(), KernelFsEND = KernelInfo.end();
      KernelFsIT != KernelFsEND; ++KernelFsIT) {
    const bool matchedWI = (KWorkitems.count(KernelFsIT->first) > 0);
    const bool matchedSi = (KSingle.count(KernelFsIT->first) > 0);
    if ((matchedWI == true) && (matchedSi == true)) {
      // Kernel function seems to be declared as both workitem and single
      // function.
      KernelFsIT->second.Flags.isWorkItem = true;
      DEBUG(dbgs() << "Function: " << KernelFsIT->first->getName() << ": has "
          << "inconsistent declarations in metadata: seems to be declared as "
          << "both workitem and single function! Now declaring as workitem.\n");
      ErrorOccured = true;
    } else if ((matchedWI == true) && (matchedSi == false)) {
      KernelFsIT->second.Flags.isWorkItem = true;
    } else if ((matchedWI == false) && (matchedSi == true)) {
      KernelFsIT->second.Flags.isWorkItem = false;
    } else if ((matchedWI == false) && (matchedSi == false)) {
      KernelFsIT->second.Flags.isWorkItem = false;
      DEBUG(dbgs() << "Function: " << KernelFsIT->first->getName() << ": is "
          << "neither declared as workitem nor as single function! Now "
          << "declaring as single function.\n");
      ErrorOccured = true;
    }
  }

  return !ErrorOccured;
}

/// \brief Searches the metadata nodes for available kernel functions.
///
/// Inspects the "opencl.kernels" metadata node and its subnodes and searches
/// for available kernel functions in the currently processed module. The found
/// kernel functions can be retrieved by using the \c getDefinedKernels and the
/// \c getAllKernels functions.
bool OpenCLMDKernels::runOnModule(Module &M) {

  if (evaluateOpenCLKernelsMD(M) == false) {
    // Errors occured during detecting kernel functions
    return false;
  }

  evaluateOclaccWorkitems(M);

  return false;
}

/// \brief Returns \c true if the function pointed to by \c F is a kernel
/// function defined in the opencl.kernels metadata node.
bool OpenCLMDKernels::isKernel(const Function* const F) const {
  if (F == 0) { return false; }
  if (foundMDNOpenCLKernels == false) { return false; }

  Function* const NF = const_cast<Function*>(F);
  const auto FIT = KernelInfo.find(NF);
  if (FIT != KernelInfo.end()) {
    return FIT->second.Flags.isKernelF;
  } else {
    return false;
  }
}

/// \brief Returns \c true if the function pointed to by \c F is annotated
/// \brief as a workitem function in the metadata nodes.
///
/// If the function \c F is listed in the oclacc.workitem metadata node \c true
/// is returned. Else \c false will be returned. Especially if neither the
/// oclacc.workitem nor the oclacc.single metadata node are found \c false is
/// returned.
bool OpenCLMDKernels::isWorkitemFunction(const Function* const F) const {
  if (F == 0) { return false; }
  // Neither the oclacc.workitem nor the oclacc.single metadata node was found
  if (foundMDNOclaccWorkitems == false) { return false; }

  Function* const NF = const_cast<Function*>(F);
  const auto FIT = KernelInfo.find(NF);
  if (FIT != KernelInfo.end()) {
    return FIT->second.Flags.isWorkItem;
  } else {
    return false;
  }
}

/// \brief Returns \c true if the function pointed to by \c F is annotated
/// \brief as a single function in the metadata nodes.
///
/// If the function \c F is listed in the oclacc.single metadata node \c true
/// is returned. Else \c false will be returned. Especially if neither the
/// oclacc.workitem nor the oclacc.single metadata node are found \c false is
/// returned.
bool OpenCLMDKernels::isSingleFunction(const Function *const F) const {
  if (F == 0) { return false; }
  // Neither the oclacc.workitem nor the oclacc.single metadata node was found
  if (foundMDNOclaccWorkitems == false) { return false; }

  Function* const NF = const_cast<Function*>(F);
  const auto FIT = KernelInfo.find(NF);
  if (FIT != KernelInfo.end()) {
    return !(FIT->second.Flags.isWorkItem);
  } else {
    return false;
  }
}

/// \brief Adds all detected kernel functions to the provided vector.
/// \param list The list that should be filled with kernel function pointers.
void OpenCLMDKernels::getKernelFunctions(std::vector<Function*> &list) const {
  if (foundMDNOpenCLKernels == false) { return; }

  for (auto KernelFsIT = KernelInfo.begin(), KernelFsEND = KernelInfo.end();
      KernelFsIT != KernelFsEND; ++KernelFsIT) {
    Function *KernelF = KernelFsIT->first;
    // Check if function is declared as kernel
    if (KernelFsIT->second.Flags.isKernelF == false) {
      continue;
    }
    // Check for null pointers
    if (KernelF == 0) {
      continue;
    }
    list.push_back(KernelF);
  }
}

void OpenCLMDKernels::getWorkitemFunctions(std::vector<Function*> &list) const {
  if (foundMDNOpenCLKernels == false) { return; }
  if (foundMDNOclaccWorkitems == false) { return; }

  for (auto KernelFsIT = KernelInfo.begin(), KernelFsEND = KernelInfo.end();
      KernelFsIT != KernelFsEND; ++KernelFsIT) {
    Function *KernelF = KernelFsIT->first;
    // Check if function is declared both as kernel and workitem
    if ((KernelFsIT->second.Flags.isKernelF == false)
     || (KernelFsIT->second.Flags.isWorkItem == false)) {
      continue;
    }
    // Null pointer check...
    if (KernelF == 0) {
      continue;
    }
    list.push_back(KernelF);
  }
}

void OpenCLMDKernels::getSingleFunctions(std::vector<Function*> &list) const {
  if (foundMDNOpenCLKernels == false) { return; }
  if (foundMDNOclaccWorkitems == false) { return; }

  for (auto KernelFsIT = KernelInfo.begin(), KernelFsEND = KernelInfo.end();
      KernelFsIT != KernelFsEND; ++KernelFsIT) {
    Function *KernelF = KernelFsIT->first;
    // Check if function is declared both as kernel and workitem
    if ((KernelFsIT->second.Flags.isKernelF == false)
     || (KernelFsIT->second.Flags.isWorkItem == true)) {
      continue;
    }
    // Null pointer check...
    if (KernelF == 0) {
      continue;
    }
    list.push_back(KernelF);
  }
}

/// \brief Returns a pointer to the metadata node describing the information for
/// \brief the function \c F.
MDNode* OpenCLMDKernels::getMDNodeForFunction(const Function* const F) const {
  if (F == 0) { return 0; }
  if (foundMDNOpenCLKernels == false) { return 0; }

  Function* const NF = const_cast<Function*>(F);
  auto FIT = KernelInfo.find(NF);
  if (FIT != KernelInfo.end()) {
    return FIT->second.MDN;
  } else {
    return 0;
  }
}

/// \brief Tries to read the required workgroup size from the metadata.
///
/// Tries to read the required workgroup size for the given kernel function
/// in the given dimension from the metadata. If the metadata does not exist
/// or cannot be read for any reasons \c 0 is returned.
unsigned OpenCLMDKernels::getRequiredWorkGroupSize(const Function &F, unsigned Dimension) {
  const ConstantInt *ReqdSizeCI = getRequiredWorkGroupSizeConst(F, Dimension);
  if (ReqdSizeCI != nullptr) {
    return ReqdSizeCI->getZExtValue();
  } else {
    return 0;
  }
}
/// \brief Tries to read the required workgroup size from the metadata.
///
/// Tries to read the required workgroup size for the given kernel function
/// in the given dimension from the metadata. If the metadata does not exist
/// or cannot be read for any reasons \c 0 is returned.
const ConstantInt* OpenCLMDKernels::getRequiredWorkGroupSizeConst(const Function &F, unsigned Dimension) {
  if (isKernel(&F) == false) {
    // The rerquired workgroupsize metadata is invalid on non-kernel functions
    return nullptr;
  }
  // There are only three dimensions
  if (Dimension > 2) { return nullptr; }

  const MDNode *KernelMDNode = getMDNodeForFunction(&F);
  if (KernelMDNode == nullptr) { return nullptr; }

  if (KernelMDNode->getNumOperands() <= 1) { return nullptr; }
  for (unsigned i = 0, e = KernelMDNode->getNumOperands(); i < e; ++i) {
    const MDNode *MDOp = dyn_cast<MDNode>(KernelMDNode->getOperand(i).get());
    if (MDOp == nullptr) { continue; }

    if (MDOp->getNumOperands() != 4) { continue; }
    if (isa<MDString>(MDOp->getOperand(0)) == false) { continue; }
    const MDString *MDOpDescription = dyn_cast<MDString>(MDOp->getOperand(0));
    if (MDOpDescription == nullptr) { continue; }
    if (MDOpDescription->getString().equals(
        Loopus::KernelMDStrings::SPIR_REQD_WGSIZE) == false) { continue; }

    // Now we found the metadata node containing the workgroups sizes
    const Metadata *ReqdSizeMD = MDOp->getOperand(1 + Dimension).get();
    if (ReqdSizeMD == nullptr) { continue; }
    const ConstantInt *ReqdSize = mdconst::dyn_extract<ConstantInt>(ReqdSizeMD);
    if (ReqdSize == nullptr) { continue; }

    return ReqdSize;
  }
  return nullptr;
}

