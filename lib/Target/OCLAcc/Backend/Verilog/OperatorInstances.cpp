#include "llvm/Support/Debug.h"

#include "OperatorInstances.h"
#include "Macros.h"

#define DEBUG_TYPE "verilog"

using namespace oclacc;

bool OperatorInstances::existsOperator(const std::string OpName) const {
  return (Ops.find(OpName) != Ops.end());
}

void OperatorInstances::addOperator(const std::string HWName, const std::string OpName, unsigned Cycles) {
  op_p O;

  if (existsOperator(OpName)) {
    O = getOperator(OpName);
  } else {
    O = std::make_shared<Operator>(OpName, Cycles);
    NameMap[OpName] = O;
    Ops.insert(OpName);
  }

  HWOp[HWName] = O;

  NDEBUG("added Operator " << OpName << " for HW " << HWName << " with " << Cycles << " cycles");
}

op_p OperatorInstances::getOperator(const std::string OpName) const {

  OpMapConstItTy OI = NameMap.find(OpName);
  assert(OI != NameMap.end() && "No Name mapping");

  return OI->second;
}

bool OperatorInstances::existsOperatorForHW(const std::string HWName) const {
  return HWOp.find(HWName) != HWOp.end();
}

op_p OperatorInstances::getOperatorForHW(const std::string HWName) const {
  OpMapConstItTy OI = HWOp.find(HWName);
  if (OI == HWOp.end()) return nullptr;

  return OI->second;
}

#ifdef DEBUG_TYPE
#undef DEBUG_TYPE
#endif
