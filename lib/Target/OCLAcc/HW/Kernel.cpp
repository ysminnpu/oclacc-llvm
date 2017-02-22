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

// InOutStreams
void Component::addInOutStream(streamport_p P) { 
  assert(P->getIR() != nullptr);
  InOutStreamsMap[P->getIR()] = P;
  InOutStreams.push_back(P);
}

bool Component::containsInOutStreamForValue(const Value *V) {
  StreamMapTy::const_iterator IT = InOutStreamsMap.find(V);
  return IT != InOutStreamsMap.end();
}

const Component::StreamsTy &Component::getInOutStreams() const { 
  return InOutStreams;
}

// Unified access
const Component::PortsTy Component::getOuts(void) const {
  PortsTy Outs;
  Outs.insert(Outs.end(), OutStreams.begin(), OutStreams.end());
  Outs.insert(Outs.end(), InOutStreams.begin(), InOutStreams.end());
  Outs.insert(Outs.end(), OutScalars.begin(), OutScalars.end());
  return Outs;
}

const Component::PortsTy Component::getIns(void) const {
  PortsTy Ins;
  Ins.insert(Ins.end(), InStreams.begin(), InStreams.end());
  Ins.insert(Ins.end(), InOutStreams.begin(), InOutStreams.end());
  Ins.insert(Ins.end(), InScalars.begin(), InScalars.end());
  return Ins;
}

void Component::addConstVal(const_p p) {
  ConstVals.push_back(p);
}

const Component::ConstantsType &Component::getConstVals() const {
  return ConstVals;
}

void Component::dump() {
  outs() << "----------------------\n";
  if (isBlock()) {
    outs() << "Block " << getUniqueName() << "\n";
    Block * B = static_cast<Block *>(this);

    outs() << "if:";
    for (const Block::CondTy &C : B->getConds()) {
      outs() << " " << C.first->getUniqueName();
    }
    outs() << "\n";

    outs() << "if not:";
    for (const Block::CondTy &C : B->getNegConds()) {
      outs() << " " << C.first->getUniqueName();
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

void Block::addCond(base_p P, block_p B) {
  Conds.push_back(std::make_pair(P, B));
}

void Block::addNegCond(base_p P, block_p B) {
  NegConds.push_back(std::make_pair(P, B));
}

const Block::CondListTy& Block::getConds() const {
  return Conds;
}

// There must be a single condition to reach the Block from B
const base_p Block::getCondForBlock(block_p B) const {
  base_p R = nullptr;
  for (const Block::CondTy &C : getConds()) {
    if (C.second == B) {
      assert(!R);
      R = C.first;
    }
  }
  return R;
}

const Block::CondListTy& Block::getNegConds() const {
  return NegConds;
}

const base_p Block::getNegCondForBlock(block_p B) const {
  base_p R = nullptr;
  for (const Block::CondTy &C : getNegConds()) {
    if (C.second == B) {
      assert(!R);
      R = C.first;
    }
  }
  return R;
}

bool Block::isConditional() const {
  return !(getConds().empty() 
    && getNegConds().empty());
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
