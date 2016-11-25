#include "FileHeader.h"
#include <string>
#include <sstream>

std::string header() {
  std::stringstream S;

  S << "`default_nettype none\n";
  S << "\n";

  return S.str();
}
