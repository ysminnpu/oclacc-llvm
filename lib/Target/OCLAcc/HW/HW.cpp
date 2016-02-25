#include <sstream>
#include <cxxabi.h>

#include "llvm/Support/Debug.h"

#include "HW.h"

namespace oclacc {

const std::string HW::dump(const std::string &Indent) const {
  std::stringstream ss;
  ss << Indent << abi::__cxa_demangle(typeid(this).name(),0,0,NULL) << " " << getUniqueName() << "\n";
  ss << Indent << "\tin:";
  for (auto I : Ins)
    ss << " " << I->getUniqueName();

  ss << Indent << "\n\tout:";
  for (auto O : Outs)
    ss << " " << O->getUniqueName();
  return ss.str();
}

} //end ns oclacc
