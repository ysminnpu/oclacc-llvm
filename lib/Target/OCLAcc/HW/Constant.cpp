#include "Constant.h"
#include <sstream>
#include <cxxabi.h>

#define TYPENAME(x) abi::__cxa_demangle(typeid(x).name(),0,0,NULL)

namespace oclacc {

ConstVal::ConstVal(const std::string Name, const std::string Bits, size_t W) : ConstVal(Name, Unsigned, Bits, W) {

}

/// \brief Constant 
///
/// Name will be changed according to the following examples:
/// - 0001        : 1
/// - 0123.456000 : 123.456
/// - 0000.5000      : 0.5
ConstVal::ConstVal(const std::string Name, Datatype T, const std::string Bits, size_t W) : HW(Name, W), T(T), Bits(Bits) {
  std::string NewName = Name;

  // Delete leading zero
  std::string::size_type i = 0, Length = NewName.length()-1;

  // If we have a real number, skip the zero before the dot
  std::string::size_type DotPos = NewName.find('.');

  if (DotPos != std::string::npos) {
    Length = DotPos-1;
  }

  for (; i < Length; ++i) {
    if (Name[i] != '0') break;
  }
  NewName.erase(0, i);

  // Delete trailing zero for floating point numbers
  DotPos = NewName.find('.');
  if (DotPos != std::string::npos) {
    for (i = Name.length()-1; i > DotPos+1; --i) {
      if (NewName[i] != '0' && NewName[i] != 0) break;
    }
    i++;
    NewName.erase(i);
  }


  HW::Name = NewName;
}

const std::string ConstVal::dump(const std::string &Indent) const {
  std::stringstream ss;
  ss << Indent << getName() << Strings_Datatype[T] << Bits;
  return ss.str();
}

#undef TYPENAME

} //end namespace oclacc
