#ifndef MEM_H
#define MEM_H

#include "HW.h"

namespace oclacc {

class Reg : public HW
{
  public:
    Reg(const std::string &Name) : HW(Name, 0)
    {
    }

    DECLARE_VISIT
};

class Ram : public HW
{
  public:
    const size_t D;
    base_p index;

    Ram(const std::string &Name, base_p index, size_t W, size_t D) : HW(Name, W), D(D), index(index)
    {
      index->appOut(base_p(this));
    }
    DECLARE_VISIT
};

class Fifo : public HW
{
  public:
    const size_t D;

    Fifo(const std::string &Name, size_t D ) : HW(Name, 0), D(D)
    {
    }
    DECLARE_VISIT
};

}

#endif /* MEM_H */
