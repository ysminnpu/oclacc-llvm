#include "Kernel.h"
#include "Utils.h"

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


// Unified access
const Component::PortsTy Kernel::getOuts(void) const {
  PortsTy Outs;
  Outs.insert(Outs.end(), OutScalars.begin(), OutScalars.end());
  for (streamport_p S : Streams) {
    if (S->hasStores())
      Outs.push_back(S);
  }
  return Outs;
}

const Component::PortsTy Kernel::getIns(void) const {
  PortsTy Ins;
  Ins.insert(Ins.end(), InScalars.begin(), InScalars.end());
  for (streamport_p S : Streams) {
    if (S->hasLoads())
      Ins.push_back(S);
  }
  return Ins;
}

const Component::PortsTy Kernel::getPorts(void) const {
  PortsTy P;
  P.insert(P.end(), InScalars.begin(), InScalars.end());
  P.insert(P.end(), OutScalars.begin(), OutScalars.end());
  P.insert(P.end(), Streams.begin(), Streams.end());
  return P;
}

void Component::addConstVal(const_p p) {
  ConstVals.push_back(p);
}

const Component::ConstantsType &Component::getConstVals() const {
  return ConstVals;
}

void Kernel::dump() {
  outs() << Line << "\n";
  outs() << "Kernel " << getUniqueName() << "\n";
  outs() << Line << "\n";

  outs() << "InScalars:\n";
  for (const scalarport_p HWP : getInScalars()) {
    outs() << " "<< HWP->getUniqueName() << "\n";
  }
  outs() << "Streams:\n";
  for (const streamport_p HWP : getStreams()) {
    outs() << " "<< HWP->getUniqueName() << "\n";
    for (const streamindex_p HWI : HWP->getLoads()) {
      outs() << "  ld @"<< HWI->getUniqueName() << "\n";
    }
    for (const streamindex_p HWI : HWP->getStores()) {
      outs() << "  st @"<< HWI->getUniqueName() << "\n";
    }
  }
  outs() << "OutScalars:\n";
  for (const scalarport_p HWP : getOutScalars()) {
    outs() << " "<< HWP->getUniqueName() << "\n";
  }
  outs() << Line << "\n";
}

void Block::dump() {
  outs() << Line << "\n";
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
  outs() << Line << "\n";

  outs() << "InScalars:\n";
  for (const scalarport_p HWP : getInScalars()) {
    outs() << " "<< HWP->getUniqueName() << "\n";
  }

  outs() << "InStreamIndices:\n";
  const StreamIndicesTy I = InStreamIndices;
  for (const streamindex_p HWP : InStreamIndices) {
    const streamport_p S = HWP->getStream();
    outs() << " "<< S->getUniqueName() << "@" << HWP->getUniqueName() << "\n";
  }

  outs() << "OutScalars:\n";
  for (const scalarport_p HWP : OutScalars) {
    outs() << " "<< HWP->getUniqueName() << "\n";
  }
  outs() << "OutStreamIndices:\n";
  for (const streamindex_p HWP : OutStreamIndices) {
    const streamport_p S = HWP->getStream();
    outs() << " "<< S->getUniqueName() << "@" << HWP->getUniqueName() << "\n";
  }

  outs() << "----------------------\n";
}

//
// Block
//
Block::Block (const std::string &Name, bool EntryBlock) : Component(Name), EntryBlock(EntryBlock) { }

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

bool Block::isEntryBlock() const {
  return EntryBlock;
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

// InStreams
void Block::addInStreamIndex(streamindex_p P) { 
  assert(P->getIR() != nullptr);
  InStreamIndicesMap[P->getIR()] = P;
  InStreamIndices.push_back(P);
}

bool Block::containsInStreamIndexForValue(const Value *V) {
  StreamIndexMapTy::const_iterator IT = InStreamIndicesMap.find(V);
  return IT != InStreamIndicesMap.end();
}

const Block::StreamIndicesTy &Block::getInStreamIndices() const {
  return InStreamIndices; 
}

// OutStreams
void Block::addOutStreamIndex(streamindex_p P) { 
  assert(P->getIR() != nullptr);
  OutStreamIndicesMap[P->getIR()] = P;
  OutStreamIndices.push_back(P);
}

bool Block::containsOutStreamIndexForValue(const Value *V) {
  StreamIndexMapTy::const_iterator IT = OutStreamIndicesMap.find(V);
  return IT != OutStreamIndicesMap.end();
}

const Block::StreamIndicesTy &Block::getOutStreamIndices() const {
  return OutStreamIndices;
}

// 
// Kernel
//
Kernel::Kernel (const std::string &Name, bool WorkItem) : Component(Name), WorkItem(WorkItem) { }

bool Kernel::isBlock() { return false; }
bool Kernel::isKernel() { return true; }

void Kernel::addBlock(block_p p) { Blocks.push_back(p); }
const Kernel::BlocksTy &Kernel::getBlocks() const { return Blocks; }

void Kernel::setWorkItem(bool T) { WorkItem = T; }
bool Kernel::isWorkItem() const { return WorkItem; }

// Streams
void Kernel::addStream(streamport_p P) { 
  assert(P->getIR() != nullptr);
  StreamsMap[P->getIR()] = P;
  Streams.push_back(P);
}

const Kernel::StreamsTy Kernel::getStreams() const { 
  return Streams; 
}

#if 0
// InStreams
void Kernel::addInStream(streamport_p P) { 
  assert(P->getIR() != nullptr);
  InStreamsMap[P->getIR()] = P;
  InStreams.push_back(P);
}

bool Kernel::containsInStreamForValue(const Value *V) {
  StreamMapTy::const_iterator IT = InStreamsMap.find(V);
  return IT != InStreamsMap.end();
}

const Kernel::StreamsTy Kernel::getInStreams() const { 
  return InStreams; 
}

// OutStreams
void Kernel::addOutStream(streamport_p P) { 
  assert(P->getIR() != nullptr);
  OutStreamsMap[P->getIR()] = P;
  OutStreams.push_back(P);
}

bool Kernel::containsOutStreamForValue(const Value *V) {
  StreamMapTy::const_iterator IT = OutStreamsMap.find(V);
  return IT != OutStreamsMap.end();
}

const Kernel::StreamsTy &Kernel::getOutStreams() const { 
  return OutStreams;
}

// InOutStreams
void Kernel::addInOutStream(streamport_p P) { 
  assert(P->getIR() != nullptr);
  InOutStreamsMap[P->getIR()] = P;
  InOutStreams.push_back(P);
}

bool Kernel::containsInOutStreamForValue(const Value *V) {
  StreamMapTy::const_iterator IT = InOutStreamsMap.find(V);
  return IT != InOutStreamsMap.end();
}

const Kernel::StreamsTy &Kernel::getInOutStreams() const { 
  return InOutStreams;
}
#endif
