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

#include "../Backend/Vhdl/VhdlTargetMachine.h"
#include "../Backend/Verilog/VerilogTargetMachine.h"
#include "../Backend/Dot/DotTargetMachine.h"
using namespace llvm;

Target llvm::TheVhdlTarget;
Target llvm::TheVerilogTarget;
Target llvm::TheDotTarget;

static bool OCLAcc_TripleMatchQuality(Triple::ArchType Arch) {
  // This class always works, but shouldn't be the default in most cases.
  return 1;
}

extern "C" void LLVMInitializeOCLAccTargetInfo() { 
  TargetRegistry::RegisterTarget(TheVhdlTarget, "vhdl",    
                                  "OCLAcc Accelerator in VHDL",
                                  &OCLAcc_TripleMatchQuality);

  TargetRegistry::RegisterTarget(TheVerilogTarget, "verilog",    
                                  "OCLAcc Accelerator in Verilog",
                                  &OCLAcc_TripleMatchQuality);

  TargetRegistry::RegisterTarget(TheDotTarget, "dot",    
                                  "OCLAccHW Dot-Graph",
                                  &OCLAcc_TripleMatchQuality);
}

extern "C" void LLVMInitializeOCLAccTargetMC() {}

