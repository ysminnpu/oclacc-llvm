#include "llvm/Support/Debug.h"

#include "OperatorInstances.h"

using namespace oclacc;

bool OperatorInstances::existsOperator(const std::string OpName) {
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

}

op_p OperatorInstances::getOperator(const std::string OpName) {
  assert(existsOperator(OpName) && "No Operator");

  OpMapConstItTy OI = NameMap.find(OpName);
  assert(OI != NameMap.end() && "No Name mapping");

  return OI->second;
}

bool OperatorInstances::existsOperatorForHW(const std::string HWName) {
  return HWOp.find(HWName) != HWOp.end();
}

op_p OperatorInstances::getOperatorForHW(const std::string HWName) {
  assert(existsOperatorForHW(HWName));

  OpMapConstItTy OI = HWOp.find(HWName);

  return OI->second;
}

