#include "llvm/Support/Debug.h"

#define TODO(x) DEBUG_WITH_TYPE("", dbgs() << "[TODO] " << __FILE__ << ":" << __LINE__ << x << "\n");

#define OCL_ERR(x) DEBUG_WITH_TYPE("", errs() << "[OCL_ERR] " << __FILE__ << ":" << __LINE__ << x << "\n");
