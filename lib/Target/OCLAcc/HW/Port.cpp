#include "Port.h"
#include "Kernel.h"

using namespace oclacc;

Port::Port(const std::string &Name, unsigned W, const Datatype &T, bool Pipelined=false) 
  : HW(Name, W), PortType(T), Pipelined(Pipelined) { }

/// Scalar Port
///
ScalarPort::ScalarPort(const std::string &Name, unsigned W, const Datatype &T, bool Pipelined) : Port(Name, W, T, Pipelined){ }




/// Stream Port
///
StreamPort::StreamPort(const std::string &Name, unsigned W, ocl::AddressSpace AS, const Datatype &T, unsigned Length) 
  : Port(Name, W, T), AddressSpace(AS), Length(Length) { }

// No inline to break dependency between Stream and StreamAccess
StreamAccess::StreamAccess(const std::string &Name, unsigned BitWidth, streamindex_p Index) : HW(Name, BitWidth) {
  Ins.push_back(Index);
}

streamport_p StreamAccess::getStream() const {
  return getIndex()->getStream();
}

streamindex_p StreamAccess::getIndex() const {
  return std::static_pointer_cast<StreamIndex>(Ins[0]);
}

const StreamPort::AccessListTy StreamPort::getAccessList(block_p HWB) const {
  AccessListTy L;
  for (const streamaccess_p A : AccessList) {
    if (A->getParent() == HWB)
      L.push_back(A);
  }
  return L;
}

bool StreamPort::hasLoads() const {
  for (const streamaccess_p S : AccessList) {
    if (S->isLoad()) {
      return true;
    }
  }
  return false;
}

bool StreamPort::hasStores() const {
  for (const streamaccess_p S : AccessList) {
    if (S->isStore()) {
      return true;
    }
  }
  return false;
}

/// \brief StreamIndex
///
/// In and Out used for data while the index depends on the actual subcalss
/// type.
StreamIndex::StreamIndex(const std::string &Name, streamport_p Stream, unsigned BitWidth) : HW(Name,BitWidth), Stream(Stream) {
}


DynamicStreamIndex::DynamicStreamIndex(const std::string &Name, streamport_p Stream, base_p Index, unsigned BitWidth) : StreamIndex(Name, Stream, BitWidth) {
  addIn(Index);
}

/// \brief Stream with compile-time constant Index 
///
/// The index is not represented as ConstantVal object to simplify offset
/// analysis for stream optimization.
StaticStreamIndex::StaticStreamIndex(const std::string &Name, streamport_p Stream, const_p ConstVal, unsigned BitWidth) : StreamIndex(Name, Stream, BitWidth) {
  setIndex(ConstVal);
}

const StreamPort::LoadListTy StreamPort::getDynamicLoads() const {
  StreamPort::LoadListTy L;

  for (const streamaccess_p S : AccessList) {
    if (S->isLoad() && std::dynamic_pointer_cast<DynamicStreamIndex>(S->getIndex()))
      L.push_back(std::static_pointer_cast<LoadAccess>(S));
  }

  return L;
}

const StreamPort::LoadListTy StreamPort::getStaticLoads() const {
  StreamPort::LoadListTy L;

  for (const streamaccess_p S : AccessList) {
    if (S->isLoad() && std::dynamic_pointer_cast<StaticStreamIndex>(S->getIndex()))
      L.push_back(std::static_pointer_cast<LoadAccess>(S));
  }

  return L;
}

const StreamPort::StoreListTy StreamPort::getDynamicStores() const {
  StreamPort::StoreListTy L;

  for (const streamaccess_p S : AccessList) {
    if (S->isStore() && std::dynamic_pointer_cast<DynamicStreamIndex>(S->getIndex()))
      L.push_back(std::static_pointer_cast<StoreAccess>(S));
  }

  return L;
}

const StreamPort::StoreListTy StreamPort::getStaticStores() const {
  StreamPort::StoreListTy L;

  for (const streamaccess_p S : AccessList) {
    if (S->isStore() && std::dynamic_pointer_cast<StaticStreamIndex>(S->getIndex()))
      L.push_back(std::static_pointer_cast<StoreAccess>(S));
  }

  return L;
}

