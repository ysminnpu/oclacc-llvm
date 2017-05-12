#ifndef MACROS_H
#define MACROS_H

#include "llvm/Support/Debug.h"

#define NO_COPY_ASSIGN(TypeName) \
  TypeName(const TypeName&)=delete;      \
  TypeName &operator=(const TypeName &)=delete;


#define TODO(x) DEBUG_WITH_TYPE("todo", dbgs() << "[TODO] " << __FILE__ << ":" << __LINE__ << " " << x << "\n");

#define OCL_ERR(x) DEBUG_WITH_TYPE("opencl", errs() << "[OCL_ERR] " << __FILE__ << ":" << __LINE__ << " " << x << "\n");

#define ODEBUG(x) DEBUG(llvm::dbgs() << "[" << DEBUG_TYPE << "] " << x << "\n")

#define DEBUG_FUNC do {DEBUG(dbgs() << __PRETTY_FUNCTION__ << ": " << R.getUniqueName() << "\n");} while (0);

#endif /* MACROS_H */
