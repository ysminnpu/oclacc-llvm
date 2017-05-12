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

}

#endif /* OPENCLDEFINES_H */
