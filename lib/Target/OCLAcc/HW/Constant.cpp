#include "Constant.h"
#include <sstream>
#include <cxxabi.h>

#define TYPENAME(x) abi::__cxa_demangle(typeid(x).name(),0,0,NULL)

namespace oclacc {

ConstVal::ConstVal(const std::string Name, const std::string Bits, size_t W) : ConstVal(Name, Unsigned, Bits, W) {

}

ConstVal::ConstVal(const std::string Name, Datatype T, const std::string Bits, size_t W) : HW(Name, W), T(T), Bits(Bits) {
  std::string NewName = Name;
  // skip Name[0] to keep constant zero
  size_t i;
  for (i = Name.length()-1; i > 0; i--) {
    if (Name[i] != '0') break;
  }
  i++;
  NewName.erase(i);

  HW::Name = NewName;
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
