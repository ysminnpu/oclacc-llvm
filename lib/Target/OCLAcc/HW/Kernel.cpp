#include "Kernel.h"

using namespace oclacc;

//
// Component
//
Component::Component(const std::string &Name) : Identifiable(Name) { }
Component::~Component() { }

const llvm::Value * Component::getIR() const { return IR; }
void Component::setIR(const llvm::Value *P) { IR=P; }

// InScalars
void Component::addInScalar(scalarport_p P) { 
  assert(P->getIR() != nullptr);
  InScalarsMap[P->getIR()] = P; 
  InScalars.push_back(P);
}

const Component::ScalarsTy &Component::getInScalars() const { 
  return InScalars; 
}

bool Component::containsInScalarForValue(const Value *V) {
  ScalarMapTy::const_iterator IT = InScalarsMap.find(V);
  return IT != InScalarsMap.end();
}

scalarport_p Component::getInScalarForValue(const Value *V) {
  ScalarMapTy::const_iterator IT = InScalarsMap.find(V);
  if (IT != InScalarsMap.end())
    return IT->second;
  else return nullptr;
}

// InStreams
void Component::addInStream(streamport_p P) { 
  assert(P->getIR() != nullptr);
  InStreamsMap[P->getIR()] = P;
  InStreams.push_back(P);
}

bool Component::containsInStreamForValue(const Value *V) {
  StreamMapTy::const_iterator IT = InStreamsMap.find(V);
  return IT != InStreamsMap.end();
}

const Component::StreamsTy Component::getInStreams() const { 
  return InStreams; 
}

// OutScalars
void Component::addOutScalar(scalarport_p P) { 
  assert(P->getIR() != nullptr);
  OutScalarsMap[P->getIR()] = P;
  OutScalars.push_back(P);
}

const Component::ScalarsTy &Component::getOutScalars() const { 
  return OutScalars;
}

bool Component::containsOutScalarForValue(const Value *V) {
  ScalarMapTy::const_iterator IT = OutScalarsMap.find(V);
  return IT != OutScalarsMap.end();
}

scalarport_p Component::getOutScalarForValue(const Value *V) {
  ScalarMapTy::const_iterator IT = OutScalarsMap.find(V);
  if (IT != OutScalarsMap.end())
    return IT->second;
  else return nullptr;
}

// OutStreams
void Component::addOutStream(streamport_p P) { 
  assert(P->getIR() != nullptr);
  OutStreamsMap[P->getIR()] = P;
  OutStreams.push_back(P);
}

bool Component::containsOutStreamForValue(const Value *V) {
  StreamMapTy::const_iterator IT = OutStreamsMap.find(V);
  return IT != OutStreamsMap.end();
}

const Component::StreamsTy &Component::getOutStreams() const { 
  return OutStreams;
}

Component::PortsTy Component::getOuts(void) const {
  PortsTy Outs;
  Outs.insert(Outs.end(), OutStreams.begin(), OutStreams.end());
  Outs.insert(Outs.end(), OutScalars.begin(), OutScalars.end());
  return Outs;
}

Component::PortsTy Component::getIns(void) const {
  PortsTy Ins;
  Ins.insert(Ins.end(), InStreams.begin(), InStreams.end());
  Ins.insert(Ins.end(), InScalars.begin(), InScalars.end());
  return Ins;
}

void Component::addConstVal(const_p p) { ConstVals.push_back(p); }
const Component::ConstantsType &Component::getConstVals() const { return ConstVals; }

void Component::dump() {
  outs() << "----------------------\n";
  if (isBlock()) {
    outs() << "Block " << getUniqueName() << "\n";
    Block * B = static_cast<Block *>(this);

    outs() << "if:";
    for (base_p C : B->getConditions()) {
      outs() << " " << C->getUniqueName();
    }
    outs() << "\n";

    outs() << "if not:";
    for (base_p C : B->getConditionNegs()) {
      outs() << " " << C->getUniqueName();
    }
    outs() << "\n";
  }
  else if (isKernel())
    outs() << "Kernel " << getUniqueName() << "\n";
  outs() << "----------------------\n";

  outs() << "InScalars:\n";
  for (const scalarport_p HWP : getInScalars()) {
    outs() << " "<< HWP->getUniqueName() << "\n";
  }
  outs() << "InStreams:\n";
  for (const streamport_p HWP : getInStreams()) {
    outs() << " "<< HWP->getUniqueName() << "\n";
  }
  outs() << "OutScalars:\n";
  for (const scalarport_p HWP : getOutScalars()) {
    outs() << " "<< HWP->getUniqueName() << "\n";
  }
  outs() << "OutStreams:\n";
  for (const streamport_p HWP : getOutStreams()) {
    outs() << " "<< HWP->getUniqueName() << "\n";
  }
  outs() << "----------------------\n";
}

//
// Block
//
Block::Block (const std::string &Name) : Component(Name) { }

bool Block::isBlock() { return true; }
bool Block::isKernel() { return false; }

void Block::addOp(base_p P) { Ops.push_back(P); }
const std::vector<base_p> &Block::getOps() const { return Ops; }

kernel_p Block::getParent() const {
  return Parent;
}

void Block::setParent(kernel_p P) {
  Parent = P;
}

void Block::addCondition(base_p P) {
  Conds.push_back(P);
}

void Block::addConditionNeg(base_p P) {
  CondNegs.push_back(P);
}

const Block::CondTy& Block::getConditions() {
  return Conds;
}

const Block::CondTy& Block::getConditionNegs() {
  return CondNegs;
}


// 
// Kernel
//
Kernel::Kernel (const std::string &Name, bool WorkItem) : Component(Name), WorkItem(WorkItem) { }

bool Kernel::isBlock() { return false; }
bool Kernel::isKernel() { return true; }

void Kernel::addBlock(block_p p) { Blocks.push_back(p); }
const std::vector<block_p> &Kernel::getBlocks() const { return Blocks; }

void Kernel::setWorkItem(bool T) { WorkItem = T; }
bool Kernel::isWorkItem() const { return WorkItem; }
