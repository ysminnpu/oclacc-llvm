#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/CommandLine.h"

#include <memory>
#include <sstream>
#include <cmath>
#include <unordered_set>
#include <map>
#include <set>

#include "../../HW/Kernel.h"

#include "Flopoco.h"
#include "Verilog.h"
#include "VerilogModule.h"
#include "Naming.h"
#include "VerilogMacros.h"
#include "DesignFiles.h"
#include "OperatorInstances.h"

#define DEBUG_TYPE "verilog"

using namespace oclacc;

VerilogModule::VerilogModule(Component &C) : Comp(C) {
}

VerilogModule::~VerilogModule() {
}

void VerilogModule::genTestBench() const {
}

const std::string VerilogModule::declFooter() const {
  std::stringstream S;
  S << "endmodule " << " // " << Comp.getUniqueName() << "\n";
  return S.str();
}




#ifdef DEBUG_TYPE
#undef DEBUG_TYPE
#endif
