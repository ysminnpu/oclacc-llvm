//===-- OCLAccTargetInfo.cpp - OCLAcc Target Implementation -------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "OCLAccTargetMachine.h"
#include "llvm/IR/Module.h"
#include "llvm/Support/TargetRegistry.h"
using namespace llvm;

Target llvm::TheOCLAccVhdlTarget;
Target llvm::TheOCLAccVerilogTarget;

static bool OCLAcc_TripleMatchQuality(Triple::ArchType Arch) {
  // This class always works, but shouldn't be the default in most cases.
  return 1;
}

extern "C" void LLVMInitializeOCLAccTargetInfo() { 
  TargetRegistry::RegisterTarget(TheOCLAccVhdlTarget, "oclacc-vhdl",    
                                  "OCLAcc Accelerator in VHLD",
                                  &OCLAcc_TripleMatchQuality);

  TargetRegistry::RegisterTarget(TheOCLAccVerilogTarget, "oclacc-verilog",    
                                  "OCLAcc Accelerator in Verilog",
                                  &OCLAcc_TripleMatchQuality);
}

extern "C" void LLVMInitializeOCLAccTargetMC() {}

