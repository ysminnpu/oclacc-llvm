#include "llvm/Support/Debug.h"

#define TODO(x) DEBUG_WITH_TYPE("todo", dbgs() << "[TODO] " << __FILE__ << ":" << __LINE__ << " " << x << "\n");

#define OCL_ERR(x) DEBUG_WITH_TYPE("opencl", errs() << "[OCL_ERR] " << __FILE__ << ":" << __LINE__ << " " << x << "\n");
