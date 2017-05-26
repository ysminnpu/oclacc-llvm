#ifndef OPENCLDEFINES_H
#define OPENCLDEFINES_H

namespace ocl {

enum AddressSpace {
  AS_PRIVATE  = 0,
  AS_GLOBAL   = 1,
  AS_CONSTANT = 2,
  AS_LOCAL    = 3,
  AS_GENERIC  = 4
};

static const std::string Strings_AddressSpace[] {
  "AS_PRIVATE",  // 0
  "AS_GLOBAL",   // 1
  "AS_CONSTANT", // 2
  "AS_LOCAL",    // 3
  "AS_GENERIC"   // 4
};


enum cl_mem_fence_flags {
  CLK_GLOBAL_MEM_FENCE = 1<<0,
  CLK_LOCAL_MEM_FENCE = 1<<1,
  CLK_IMAGE_MEM_FENCE = 1<<2
};

enum  memory_scope {
  memory_scope_work_item,
  memory_scope_work_group,
  memory_scope_device,
  memory_scope_all_svm_devices,
  memory_scope_sub_group
};

} // end ns ocl

#endif /* OPENCLDEFINES_H */
