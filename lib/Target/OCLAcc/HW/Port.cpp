#include "Port.h"

using namespace oclacc;

Port::Port(const std::string &Name, 
    size_t W, const Datatype &T, 
    bool Pipelined=false) : HW(Name, W), PortType(T), Pipelined(Pipelined) { }

const Datatype &Port::getPortType() { return PortType; }

bool Port::isPipelined(void) const { return Pipelined; }

/// Scalar Port
///
ScalarPort::ScalarPort(const std::string &Name, size_t W, const Datatype &T, bool Pipelined) : Port(Name, W, T, Pipelined){ }

bool ScalarPort::isScalar() const { return true; }



/// Stream Port
///
StreamPort::StreamPort(const std::string &Name, size_t W, ocl::AddressSpace, const Datatype &T) : Port(Name, W, T) { }

bool StreamPort::isScalar() const { return false; }

void StreamPort::addLoad(streamindex_p I) {
  IndexList.push_back(I);
  AccessList.push_back(Load);
}

const StreamPort::IndexListTy StreamPort::get(StreamPort::AccessTy A) const {
  IndexListTy L;
  for (IndexListTy::size_type i = 0; i < IndexList.size(); ++i) {
    if (AccessList[i] == A) {
      L.push_back(IndexList[i]);
    }
  }
  return L;
}

const StreamPort::IndexListTy StreamPort::getLoads() const {
  return get(Load);
}

const StreamPort::IndexListTy StreamPort::getStores() const {
  return get(Store);
}


bool StreamPort::hasLoads() const {
  for (AccessListTy::size_type i = 0; i < AccessList.size(); ++i) {
    if (AccessList[i] == Load) {
      return true;
    }
  }
  return false;
}

bool StreamPort::hasStores() const {
  for (AccessListTy::size_type i = 0; i < AccessList.size(); ++i) {
    if (AccessList[i] == Store) {
      return true;
    }
  }
  return false;
}

bool StreamPort::isLoad(StreamIndex *I) const {
  for (IndexListTy::size_type i = 0; i < IndexList.size(); ++i) {
    if (IndexList[i].get() == I && AccessList[i] == Load) {
      return true;
    }
  }
  return false;
}

void StreamPort::addStore(streamindex_p I) {
  IndexList.push_back(I);
  AccessList.push_back(Store);
}

bool StreamPort::isStore(StreamIndex *I) const {
  for (IndexListTy::size_type i = 0; i < IndexList.size(); ++i) {
    if (IndexList[i].get() == I && AccessList[i] == Store) {
      return true;
    }
  }
  return false;
}

const StreamPort::IndexListTy &StreamPort::getIndexList() const {
  return IndexList;
}

const StreamPort::AccessListTy &StreamPort::getAccessList() const {
  return AccessList;
}

/// \brief StreamIndex
///
/// In and Out used for data while the index depends on the actual subcalss
/// type.
StreamIndex::StreamIndex(const std::string &Name, streamport_p Stream) : HW(Name,0), Stream(Stream) {
}

const streamport_p StreamIndex::getStream() const {
  return Stream;
}


DynamicStreamIndex::DynamicStreamIndex(const std::string &Name, streamport_p Stream, base_p Index) : StreamIndex(Name, Stream), Index(Index) { }

void DynamicStreamIndex::setIndex(base_p I) {
  Index = I;
}

base_p DynamicStreamIndex::getIndex() const { return Index; }

bool DynamicStreamIndex::isStatic() const { return false;}

/// \brief Stream with compile-time constant Index 
///
/// The index is not represented as ConstantVal object to simplify offset
/// analysis for stream optimization.
StaticStreamIndex::StaticStreamIndex(const std::string &Name, streamport_p Stream, int64_t Index, size_t W) : StreamIndex(Name, Stream), Index(Index) {
  setBitWidth(W);
}

void StaticStreamIndex::setIndex(StaticStreamIndex::IndexTy I) {
  Index = I;
}
StaticStreamIndex::IndexTy StaticStreamIndex::getIndex() const { return Index; }

bool StaticStreamIndex::isStatic() const {return true;}

const std::string StaticStreamIndex::getUniqueName() const {
  return getName();
}

const StreamPort::StaticIndexListTy StreamPort::getStaticIndizes() const {
  StreamPort::StaticIndexListTy R;

  for (IndexListTy::size_type i = 0; i < IndexList.size(); ++i) {
    if (IndexList[i]->isStatic()) {
      R.push_back(std::static_pointer_cast<StaticStreamIndex>(IndexList[i]));
    }
  }

  return R;
}

const StreamPort::StaticIndexListTy StreamPort::getStaticLoads() const {
  StreamPort::StaticIndexListTy R;

  for (IndexListTy::size_type i = 0; i < IndexList.size(); ++i) {
    if (IndexList[i]->isStatic() && AccessList[i] == AccessTy::Load) {
      R.push_back(std::static_pointer_cast<StaticStreamIndex>(IndexList[i]));
    }
  }

  return R;
}

const StreamPort::StaticIndexListTy StreamPort::getStaticStores() const {
  StreamPort::StaticIndexListTy R;

  for (IndexListTy::size_type i = 0; i < IndexList.size(); ++i) {
    if (IndexList[i]->isStatic() && AccessList[i] == AccessTy::Store) {
      R.push_back(std::static_pointer_cast<StaticStreamIndex>(IndexList[i]));
    }
  }

  return R;
}

const StreamPort::DynamicIndexListTy StreamPort::getDynamicIndizes() const {
  StreamPort::DynamicIndexListTy R;

  for (IndexListTy::size_type i = 0; i < IndexList.size(); ++i) {
    if (!IndexList[i]->isStatic()) {
      R.push_back(std::static_pointer_cast<DynamicStreamIndex>(IndexList[i]));
    }
  }

  return R;
}

const StreamPort::DynamicIndexListTy StreamPort::getDynamicLoads() const {
  StreamPort::DynamicIndexListTy R;

  for (IndexListTy::size_type i = 0; i < IndexList.size(); ++i) {
    if (!IndexList[i]->isStatic() && AccessList[i] == AccessTy::Load) {
      R.push_back(std::static_pointer_cast<DynamicStreamIndex>(IndexList[i]));
    }
  }

  return R;
}

const StreamPort::DynamicIndexListTy StreamPort::getDynamicStores() const {
  StreamPort::DynamicIndexListTy R;

  for (IndexListTy::size_type i = 0; i < IndexList.size(); ++i) {
    if (!IndexList[i]->isStatic() && AccessList[i] == AccessTy::Store) {
      R.push_back(std::static_pointer_cast<DynamicStreamIndex>(IndexList[i]));
    }
  }

  return R;
}
