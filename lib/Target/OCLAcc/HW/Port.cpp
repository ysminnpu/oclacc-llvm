#include "Port.h"

using namespace oclacc;

Port::Port(const std::string &Name, size_t W, const Datatype &T, bool Pipelined=false) 
  : HW(Name, W), PortType(T), Pipelined(Pipelined) { }

/// Scalar Port
///
ScalarPort::ScalarPort(const std::string &Name, size_t W, const Datatype &T, bool Pipelined) : Port(Name, W, T, Pipelined){ }

bool ScalarPort::isScalar() const { return true; }



/// Stream Port
///
StreamPort::StreamPort(const std::string &Name, size_t W, ocl::AddressSpace, const Datatype &T) : Port(Name, W, T) { }


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
StaticStreamIndex::StaticStreamIndex(const std::string &Name, streamport_p Stream, int64_t Index, size_t BitWidth) : StreamIndex(Name, Stream, BitWidth), Index(Index) {
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
