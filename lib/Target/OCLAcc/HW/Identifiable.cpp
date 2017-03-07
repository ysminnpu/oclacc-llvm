#include <string>
#include "Identifiable.h"

namespace oclacc {

unsigned int Identifiable::currUID = 0;

Identifiable::Identifiable(const std::string &Name) : UID(currUID), Name(Name)
{
  currUID++;
}

Identifiable::UIDTy Identifiable::getUID() const {
  return UID;;
}

const std::string Identifiable::getName() const {
  return Name;
}

const std::string Identifiable::getUniqueName() const {
  return Name+"_"+std::to_string(UID);;
}

void Identifiable::setName(const std::string &N) {
  Name = N;
}


} // end namespace oclacc
