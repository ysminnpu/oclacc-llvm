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

static const std::string FUN_WI_M[] {
  "_Z13get_global_idj",
  "_Z12get_work_dim",
  "_Z15get_global_sizej",
  "_Z13get_global_idj",
  "_Z14get_local_sizej",
  "_Z23get_enqueued_local_sizej",
  "_Z12get_local_idj",
  "_Z14get_num_groupsj",
  "_Z12get_group_idj",
  "_Z17get_global_offsetj",
  "_Z20get_global_linear_id",
  "_Z19get_local_linear_id"
};
const auto FUN_WI_M_B = std::begin(FUN_WI_M);
const auto FUN_WI_M_E = std::end(FUN_WI_M);

static const std::string FUN_WI[] {
  "get_global_id",
  "get_work_dim",
  "get_global_size",
  "get_global_id",
  "get_local_size",
  "get_enqueued_local_size",
  "get_local_id",
  "get_num_groups",
  "get_group_id",
  "get_global_offset",
  "get_global_linear_id",
  "get_local_linear_id"
};
const auto FUN_WI_B = std::begin(FUN_WI);
const auto FUN_WI_E = std::end(FUN_WI);


static const std::string FUN_MATH_M[] {
  "_Z4acosf",
  "_Z5acoshf"
};
const auto FUN_MATH_M_B = std::begin(FUN_MATH_M);
const auto FUN_MATH_M_E = std::end(FUN_MATH_M);

static const std::string FUN_MATH[] {
  "acos",
  "acosh"
};
const auto FUN_MATH_B = std::begin(FUN_MATH);
const auto FUN_MATH_E = std::end(FUN_MATH);

static const std::string FUN_INT_M[] {
  "_Z3absi"
};
const auto FUN_INT_M_B = std::begin(FUN_INT_M);
const auto FUN_INT_M_E = std::end(FUN_INT_M);

static const std::string FUN_INT[] {
  "abs"
};
const auto FUN_INT_B = std::begin(FUN_INT);
const auto FUN_INT_E = std::end(FUN_INT);

}

#endif /* OPENCLDEFINES_H */
