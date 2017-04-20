#ifndef CONTROL_H
#define CONTROL_H

#include <list>
#include <initializer_list>

#include "HW.h"

namespace oclacc {


class Mux : public HW
{
  public:
    typedef std::pair<port_p, base_p> MuxInputTy;
    typedef std::list<MuxInputTy> MuxInputsTy;

  private:
    MuxInputsTy Ins;

    using HW::addIn;
    using HW::getIns;

  public:
    Mux(const std::string &Name, unsigned BitWidth) : HW(Name,BitWidth) {
    }

    Mux (const Mux &) = delete;
    Mux &operator =(const Mux &) = delete;

    void addIn(port_p P, base_p B) {
      Ins.push_back(std::make_pair(P, B));
    }

    MuxInputsTy &getIns() {
      return Ins;
    }

    DECLARE_VISIT;
};

}

#endif /* CONTROL_H */
