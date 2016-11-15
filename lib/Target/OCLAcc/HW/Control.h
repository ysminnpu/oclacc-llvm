#ifndef CONTROL_H
#define CONTROL_H

#include <list>
#include <initializer_list>

#include "HW.h"

namespace oclacc {


class Tmp : public HW
{
  public:
    base_p in;
    Tmp(const std::string &Name) : HW(Name, 0)
    {
      //pass
    }

    DECLARE_VISIT;

    virtual void addIn(base_p p)
    {
      in=p;
    }
};

class Mux : public HW
{
  public:
    typedef std::pair<port_p, block_p> MuxInputTy;
    typedef std::list<MuxInputTy> MuxInputsTy;

  private:
    MuxInputsTy Ins;

    using HW::addIn;

  public:
    Mux(const std::string &Name) : HW(Name,0) {
    }

    Mux (const Mux &) = delete;
    Mux &operator =(const Mux &) = delete;

    void addIn(port_p P, block_p B) {
      Ins.push_back(std::make_pair(P, B));
    }

    MuxInputsTy &getns() {
      return Ins;
    }

    DECLARE_VISIT;
};

}

#endif /* CONTROL_H */
