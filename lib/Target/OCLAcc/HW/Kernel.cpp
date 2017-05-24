#include <map>
#include <set>
#include <vector>

#include "Constant.h"
#include "Kernel.h"
#include "Utils.h"

#define DEBUG_TYPE "kernel"

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
const Kernel::PortsTy Kernel::getOuts(void) const {
  PortsTy Outs;
  Outs.insert(Outs.end(), OutScalars.begin(), OutScalars.end());
  for (streamport_p S : Streams) {
    if (S->hasStores())
      Outs.push_back(S);
  }
  return Outs;
}

const Kernel::PortsTy Kernel::getIns(void) const {
  PortsTy Ins;
  Ins.insert(Ins.end(), InScalars.begin(), InScalars.end());
  for (streamport_p S : Streams) {
    if (S->hasLoads())
      Ins.push_back(S);
  }
  return Ins;
}

const Kernel::PortsTy Kernel::getPorts(void) const {
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
    outs() << " "<< HWP->getUniqueName() << ": " << (HWP->isPipelined()? "pipelined" : "not pipelined") << "\n";
  }
  outs() << "Streams:\n";
  for (const streamport_p HWP : getStreams()) {
    outs() << " "<< HWP->getUniqueName() << "\n";
    for (const loadaccess_p HWL : HWP->getLoads()) {
      outs() << "  ld " << HWL->getUniqueName() << "@" << HWL->getIndex()->getUniqueName() << "\n";
    }
    for (const storeaccess_p HWS : HWP->getStores()) {
      outs() << "  st " << HWS->getUniqueName() << "@" << HWS->getIndex()->getUniqueName() << "\n";
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
    outs() << " "<< HWP->getUniqueName() << ": " << (HWP->isPipelined()? "pipelined" : "not pipelined") << "\n";
  }
  outs() << "OutScalars:\n";
  for (const scalarport_p HWP : OutScalars) {
    outs() << " "<< HWP->getUniqueName() << "\n";
  }

  outs() << "Loads:\n";
  for (const loadaccess_p HWL : getLoads()) {
    const streamport_p S = HWL->getStream();
    outs() << " " << HWL->getUniqueName() << ": "<< S->getUniqueName() << " @ " << HWL->getIndex()->getUniqueName() << "\n";
  }
  outs() << "Stores:\n";
  for (const storeaccess_p HWS : getStores()) {
    const streamport_p S = HWS->getStream();
    outs() << " " << HWS->getUniqueName() << ": "<< S->getUniqueName() << " @ " << HWS->getIndex()->getUniqueName() << " = " << HWS->getValue()->getUniqueName() << "\n";
  }

  outs() << Line << "\n";
}

//
// Block
//
Block::Block (const std::string &Name, bool EntryBlock) : Component(Name), EntryBlock(EntryBlock) { }

// Use Kahn's algorithm without back edge detection but a map of deleted edges
const HW::HWListTy Block::getOpsTopologicallySorted() const {
  const std::vector<scalarport_p> IS = getInScalars();

  const std::vector<loadaccess_p> LD = getLoads();
  const std::vector<storeaccess_p> ST = getStores();

  const std::vector<const_p> C = getConstVals();


  std::vector<base_p> L;
  std::set<base_p> S;
  S.insert(C.begin(), C.end());
  S.insert(IS.begin(), IS.end());
  S.insert(LD.begin(), LD.end()); // In[0] Index
  S.insert(ST.begin(), ST.end()); // In[0] Index, In[1] Value

  std::map<base_p, std::set<base_p> > DelEdges;

  while (S.size()) {
    base_p F = *(S.begin());
    S.erase(S.begin());

    L.push_back(F);
    
    for (base_p O : F->getOuts()) {
      if (O->getParent().get() != this) continue;

      bool hasOtherEdges = false;
      DelEdges[O].insert(F);

      HW::HWListTy Ins = O->getIns();

      for (base_p I : Ins) {
        if (I != F && DelEdges[O].find(I) == DelEdges[O].end()) {
          hasOtherEdges = true;
          break;
        }
      }

      if (!hasOtherEdges) {
        S.insert(O);
      }
    }
  }

  ODEBUG("Order of block " << getUniqueName() << ":");
  for (base_p P : L) {
    ODEBUG("  " << P->getUniqueName());
  }

  return L;
}

// FIXME:
// There must be a single condition to reach the Block from B. By iterating over
// all conditions, we check that invariant. Can be deleted for optimization
const scalarport_p Block::getCondReachedByBlock(block_p B) const {
  scalarport_p R = nullptr;
  for (const Block::CondTy &C : getConds()) {
    if (C.second == B) {
      assert(!R);
      R = std::static_pointer_cast<ScalarPort>(C.first);
    }
  }
  return R;
}

const scalarport_p Block::getNegCondReachedByBlock(block_p B) const {
  scalarport_p R = nullptr;
  for (const Block::CondTy &C : getNegConds()) {
    if (C.second == B) {
      assert(!R);
      R = std::static_pointer_cast<ScalarPort>(C.first);
    }
  }
  return R;
}

const Block::SingleCondTy Block::getCondForScalarPort(scalarport_p P) const {
  // EntryBlock has no conditions
  assert(!isEntryBlock());
  assert(P->getIns().size() > 1);

  Block::SingleCondTy R;

  block_p PB = std::static_pointer_cast<Block>(P->getParent());

  ODEBUG("Conditions for Scalarport " << P->getUniqueName() << "(" << PB->getUniqueName() << "):");

  // Walk through ScalarInputs, look up From and then Condition
  for (base_p I : P->getIns()) {
    scalarport_p SSP = std::static_pointer_cast<ScalarPort>(I);

    block_p IB = std::static_pointer_cast<Block>(SSP->getParent());

    scalarport_p C = PB->getCondReachedByBlock(IB);

    scalarport_p NC = PB->getNegCondReachedByBlock(IB);

    DEBUG(
        if (C) ODEBUG("  from " << SSP->getUniqueName() << ": " << C->getUniqueName() << "(" << IB->getUniqueName() << ")");
        if (NC) ODEBUG("  form " << SSP->getUniqueName() << ": !" << NC->getUniqueName() << "(" << IB->getUniqueName() << ")");
        );

    R[SSP] = std::make_pair(C,NC);
  } 

  return R;
}

// InStreams
const loadaccess_p Block::getLoadForValue(const Value *V) {
  for (const streamaccess_p A : AccessList) {
    if (A->isLoad())
      if (A->getIR() == V)
        return std::static_pointer_cast<LoadAccess>(A);
  }
  return nullptr;
}

const StreamPort::LoadListTy Block::getLoads() const {
  StreamPort::LoadListTy L;
  for (const streamaccess_p A : AccessList) {
    if (A->isLoad())
        L.push_back(std::static_pointer_cast<LoadAccess>(A));
  }

  return L;
}

// OutStreams

const storeaccess_p Block::getStoreForValue(const Value *V) {
  for (const streamaccess_p A : AccessList) {
    if (A->isStore())
      if (A->getIR() == V)
        return std::static_pointer_cast<StoreAccess>(A);
  }
    return nullptr;
}

const StreamPort::StoreListTy Block::getStores() const {
  StreamPort::StoreListTy L;
  for (const streamaccess_p A : AccessList) {
    if (A->isStore())
        L.push_back(std::static_pointer_cast<StoreAccess>(A));
  }

  return L;
}

// 
// Kernel
//
Kernel::Kernel(const std::string &Name, bool WorkItem) : Component(Name), WorkItem(WorkItem), RequiredWorkGroupSize({{0,0,0}}) { }

void Kernel::addBlock(block_p P) {
  assert(P && P->getIR());

  Blocks.insert(P);
}

void Kernel::setWorkItem(bool T) { WorkItem = T; }
bool Kernel::isWorkItem() const { return WorkItem; }

// Streams
void Kernel::addStream(streamport_p P) { 
  assert(P);
  assert(P->getIR() != nullptr);

  Streams.insert(P);
}

#ifdef DEBUG_TYPE
#undef DEBUG_TYPE
#endif
