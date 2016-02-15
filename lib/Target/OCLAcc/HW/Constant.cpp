#include "Constant.h"
#include <sstream>
#include <cxxabi.h>

#define TYPENAME(x) abi::__cxa_demangle(typeid(x).name(),0,0,NULL)

namespace oclacc {

ConstVal::ConstVal(const std::string &Name, uint64_t V, size_t W) : HW(Name, W), T(Unsigned), V(V) {
}
ConstVal::ConstVal(const std::string &Name, DataType T, uint64_t V, size_t W) : HW(Name, W), T(T), V(V) {
}

const std::string ConstVal::dump(const std::string &Indent) const {
  std::stringstream ss;
  ss << Indent << getName() << Strings_DataType[T] << V;
  return ss.str();
}

const std::string ConstVal::getUniqueName() const {
  return getName();
}

#undef TYPENAME

} //end namespace oclacc
