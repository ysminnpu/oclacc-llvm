#ifndef SYNCHRONIZATION_H
#define SYNCHRONIZATION_H

#include "HW.h"

namespace oclacc {


class Barrier : public HW
{
  private:
    ocl::cl_mem_fence_flags Flags;
    ocl::memory_scope Scope;

  public:
    using HW::addIn;
    using HW::getIns;

  public:
    Barrier(const std::string &Name,
        ocl::cl_mem_fence_flags Flags = ocl::CLK_LOCAL_MEM_FENCE,
        ocl::memory_scope Scope = ocl::memory_scope_work_group) : HW(Name,1), Flags(Flags), Scope(Scope) {
    }

    const std::string getFlagsString() {
      std::string S;
      if (Flags & ocl::CLK_GLOBAL_MEM_FENCE)
        S += "CLK_GLOBAL_MEM_FENCE ";
      if (Flags & ocl::CLK_LOCAL_MEM_FENCE)
        S += "CLK_LOCAL_MEM_FENCE ";
      if (Flags & ocl::CLK_IMAGE_MEM_FENCE)
        S += "CLK_IMAGE_MEM_FENCE ";

      return S;
    }

    const std::string getScopeString() {
      switch(Scope) {
        case ocl::memory_scope_work_item:
          return "memory_scope_work_item";
        case ocl::memory_scope_work_group:
          return "memory_scope_work_group";
        case ocl::memory_scope_device:
          return "memory_scope_device";
        case ocl::memory_scope_all_svm_devices:
          return "memory_scope_all_svm_devices";
        case ocl::memory_scope_sub_group:
          return "memory_scope_sub_group";
      }
    }

    NO_COPY_ASSIGN(Barrier);

    DECLARE_VISIT;
};

}

#endif /* SYNCHRONIZATION_H */
