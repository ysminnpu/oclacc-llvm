#include <vector>

#include "HW.h"
#include "typedefs.h"
#include "Design.h"

namespace oclacc {

DesignUnit::DesignUnit( const std::string &Name) : Identifiable(Name) {};

void DesignUnit::addKernel(kernel_p Kernel) {
  Kernels.push_back(Kernel);
}

} //end namespace oclacc
