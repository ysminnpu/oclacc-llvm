#ifndef SYNCHRONIZATION_H
#define SYNCHRONIZATION_H

#include "HW.h"

namespace oclacc {


class Barrier : public HW
{
  public:
    using HW::addIn;
    using HW::getIns;

  public:
    Barrier(const std::string &Name) : HW(Name,1) {
    }

    NO_COPY_ASSIGN(Barrier);

    DECLARE_VISIT;
};

}

#endif /* SYNCHRONIZATION_H */
