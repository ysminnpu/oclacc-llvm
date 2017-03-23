#ifndef SIGNAL_H
#define SIGNAL_H

#include <string>
#include <vector>

namespace oclacc {

enum SignalDirection {
  In,
  Out,
  Local
};

const std::string SignalDirection_S[] = {"input", "output", ""};

enum SignalType {
  Wire,
  Reg
};
const std::string SignalType_S[] = {"wire", "reg"};

struct Signal {
  std::string Name;
  unsigned BitWidth;
  SignalDirection Direction;
  SignalType Type;

  /// \brief Signal to connect blocks and components
  Signal(const std::string Name, unsigned BitWidth, SignalDirection Direction, SignalType Type);

  const std::string getDirectionStr(void) const;
  const std::string getTypeStr(void) const;
  const std::string getDefStr(void) const;
};

typedef std::vector<Signal> PortListTy;

} // end ns oclacc

#endif /* SIGNAL_H */
