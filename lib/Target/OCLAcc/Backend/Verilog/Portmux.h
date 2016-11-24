#ifndef PORTMUX_H
#define PORTMUX_H

#include <string>

namespace oclacc {

class Port;

class Portmux {
  private:
    const Port &InPort;
    unsigned NumPorts;
    unsigned Bitwidth;
    std::string ModName;
    std::string Instname;
  public:
    Portmux(const Port &);

    void definition();

    std::string instantiate();
};

} // end ns oclacc

#endif /* PORTMUX_H */
