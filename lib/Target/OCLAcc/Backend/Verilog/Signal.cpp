#include <sstream>
#include <iomanip>

#include "Signal.h"

using namespace oclacc;

Signal::Signal(const std::string Name, unsigned BitWidth, SignalDirection Direction, SignalType Type) : Name(Name), BitWidth(BitWidth), Direction(Direction), Type(Type) {
}

const std::string Signal::getDirectionStr(void) const {
  return std::string(SignalDirection_S[Direction]);
}

const std::string Signal::getTypeStr(void) const {
  return std::string(Signal::SignalType_S[Type]);
}

const std::string Signal::getDefStr(void) const {
  std::stringstream S;

  if (Direction != Local)
    S << std::setw(6) << std::left << getDirectionStr() << " ";
  
  S << std::setw(4) << std::left << getTypeStr() << " ";

  // Print width columnwise
  if (BitWidth!=1) {
    std::string BWS = "[" + std::to_string(BitWidth-1) + ":0]";

    /// Separate, otherwise setw() works incorrectly
    S << std::setw(6) << BWS;
  } else
    S << std::string(6, ' ');

  S << " " << Name;
  
  return S.str();
}

const char * Signal::SignalType_S[] = {"wire", "reg"};
const char * Signal::SignalDirection_S[] = {"input", "output", ""};
