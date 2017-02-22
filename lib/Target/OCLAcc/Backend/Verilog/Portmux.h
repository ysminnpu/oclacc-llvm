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
    std::string InstName;
    std::string FileName;
  public:
    Portmux(const Port &);

    void definition();

    std::string instantiate();

    const std::string &getFileName() {
      return FileName;
    }
};

} // end ns oclacc

#endif /* PORTMUX_H */
