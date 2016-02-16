#ifndef DESIGN_H
#define DESIGN_H

#include <vector>

#include "HW.h"
#include "typedefs.h"
#include "Identifiable.h"
#include "Visitor/Base.h"

namespace oclacc {

class DesignUnit : public Identifiable, public BaseVisitable
{
  public:
    std::vector<kernel_p> Kernels;

    DesignUnit();
    DesignUnit(const std::string &Name);
    DesignUnit (const DesignUnit &) = delete;
    DesignUnit &operator =(const DesignUnit &) = delete;

    void addKernel(kernel_p Kernel);

    DECLARE_VISIT
};

} //ns oclacc

#endif /* DESIGN_H */
