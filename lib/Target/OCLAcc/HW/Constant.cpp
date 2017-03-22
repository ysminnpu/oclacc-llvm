#include "Constant.h"
#include <sstream>
#include <cxxabi.h>

#define TYPENAME(x) abi::__cxa_demangle(typeid(x).name(),0,0,NULL)

namespace oclacc {

ConstVal::ConstVal(const std::string &Name, const std::string Bits, size_t W) : HW(Name, W), T(Unsigned), Bits(Bits) {
}
ConstVal::ConstVal(const std::string &Name, Datatype T, const std::string Bits, size_t W) : HW(Name, W), T(T), Bits(Bits) {
}

const std::string ConstVal::dump(const std::string &Indent) const {
  std::stringstream ss;
  ss << Indent << getName() << Strings_Datatype[T] << Bits;
  return ss.str();
}

const std::string ConstVal::getUniqueName() const {
  return getName();
}

const std::string &ConstVal::getBits() const {
  return Bits;
}

#undef TYPENAME

} //end namespace oclacc
