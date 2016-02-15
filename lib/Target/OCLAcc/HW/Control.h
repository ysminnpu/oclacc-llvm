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

    DECLARE_VISIT

    virtual void appIn(base_p p)
    {
      in=p;
    }
};

class Mux : public HW
{
  public:
    typedef std::pair<base_p, block_p> muxinput;
    typedef std::list<muxinput> muxinput_list;

  private:
    muxinput_list Ins;

    using HW::appIn;

  public:
    Mux(const std::string &Name) : HW(Name,0) {
    }

    Mux (const Mux &) = delete;
    Mux &operator =(const Mux &) = delete;

    void addMuxIn(base_p P, block_p B) {
      Ins.push_back(std::make_pair(P, B));
    }

    muxinput_list &getMuxIns() {
      return Ins;
    }

    DECLARE_VISIT

};

}

#endif /* CONTROL_H */
