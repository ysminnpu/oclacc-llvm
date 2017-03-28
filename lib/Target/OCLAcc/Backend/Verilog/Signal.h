#ifndef SIGNAL_H
#define SIGNAL_H

#include <string>
#include <vector>

namespace oclacc {

class Signal {
  public:
    enum SignalDirection {
      In,
      Out,
      Local
    };

    enum SignalType {
      Wire,
      Reg
    };

    std::string Name;
    unsigned BitWidth;
    SignalDirection Direction;
    SignalType Type;

    typedef std::vector<Signal> SignalListTy;
    typedef SignalListTy::const_iterator SignalListConstItTy;

  private:
    static const char *SignalType_S[];
    static const char *SignalDirection_S[];

  public:
    /// \brief Signal to connect blocks and components
    Signal(const std::string Name, unsigned BitWidth, SignalDirection Direction, Signal::SignalType Type);

    const std::string getDirectionStr(void) const;
    const std::string getTypeStr(void) const;
    const std::string getDefStr(void) const;
};

} // end ns oclacc

#endif /* SIGNAL_H */
