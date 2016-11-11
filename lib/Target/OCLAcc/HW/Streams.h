#ifndef STREAMS_H
#define STREAMS_H

#include <vector>
#include <iterator>

#include "Visitor/Visitable.h"
#include "typedefs.h"
#include "Identifiable.h"
#include "OpenCLDefines.h"

namespace oclacc {

class Port : public HW {
  protected:
    Port(const std::string &Name, size_t W, const Datatype &T) : HW(Name, W), PortType(T) {
    }

    virtual bool isScalar()=0;


  private:
    const Datatype &PortType;
  public:
    const Datatype &getPortType() {
      return PortType;
    }
};

class ScalarPort : public Port {
  public:
    ScalarPort(const std::string &Name, size_t W, const Datatype &T) : Port(Name, W, T) { }

    bool isScalar() { return true; }

    DECLARE_VISIT;
};

/// \brief Represents a Load and Store Port.
///
/// To preserve correct Load and Store order, all index operations are in the
/// same list. 
///
/// Load/Store cannot be an attribute of StreamIndex because address
/// generation happens before the actual access and a single StreamIndex may be
/// used for both.
///
class StreamPort : public Port {
  public:
    enum AccessType {
      Invalid,
      Load,
      Store
    };

    typedef std::vector<streamindex_p> IndexListType;
    typedef IndexListType::iterator IndexListIt;
    typedef IndexListType::const_iterator IndexListConstIt;
    typedef std::vector<AccessType> AccessListType;

  private:
    // must be a ordered list of indices to preserve correct store order.
    // Instead of a single list we use separate lists for the Index and the
    // access type.
    IndexListType IndexList;
    AccessListType AccessList;

  public:

    StreamPort(const std::string &Name, size_t W, ocl::AddressSpace, const Datatype &T) : Port(Name, W, T) { }
    DECLARE_VISIT;

    bool isScalar() { return false; }

    void addLoad(streamindex_p I) {
      IndexList.push_back(I);
      AccessList.push_back(Load);
    }

    const IndexListType get(AccessType A) const {
      IndexListType L;
      for (IndexListType::size_type i = 0; i < IndexList.size(); ++i) {
        if (AccessList[i] == A) {
          L.push_back(IndexList[i]);
        }
      }
      return L;
    }

    const IndexListType getLoads() const {
      return get(Load);
    }

    const IndexListType getStores() const {
      return get(Store);
    }

    bool isLoad(streamindex_p I) const {
      bool ret = false;
      IndexListConstIt It = std::find(IndexList.begin(), IndexList.end(), I);
      if (It != IndexList.end()) {
        unsigned index = std::distance(IndexList.begin(), It);
        ret = AccessList.at(index) == Load;
      }

      return ret;
    }

    void addStore(streamindex_p I) {
      IndexList.push_back(I);
      AccessList.push_back(Store);
    }

    bool isStore(streamindex_p I) const {
      bool ret = false;
      IndexListConstIt It = std::find(IndexList.begin(), IndexList.end(), I);
      if (It != IndexList.end()) {
        unsigned index = std::distance(IndexList.begin(), It);
        ret = AccessList.at(index) == Store;
      }
      
      return ret;
    }

    const IndexListType &getIndexList() const {
      return IndexList;
    }
    const AccessListType &getAccessList() const {
      return AccessList;
    }
};

/// \brief StreamIndex
///
/// In and Out used for data while the index depends on the actual subcalss
/// type.
class StreamIndex : public HW {
  private:
    streamport_p Stream;
    base_p Index;

  protected:
    StreamIndex(const std::string &Name, streamport_p Stream) : HW(Name,0), Stream(Stream) { }

  public:
    StreamIndex(const StreamIndex&) = delete;
    StreamIndex& operator=(const StreamIndex&) = delete;

    streamport_p getStream() const {
      return Stream;
    }

    virtual bool isStatic() const = 0;

    DECLARE_VISIT

};


class DynamicStreamIndex : public StreamIndex {

  private:
    base_p Index;

  public:
    DynamicStreamIndex(const std::string &Name, streamport_p Stream, base_p Index) : StreamIndex(Name, Stream), Index(Index) { }

    DynamicStreamIndex(const DynamicStreamIndex&) = delete;
    DynamicStreamIndex& operator=(const DynamicStreamIndex&) = delete;


    virtual void setIndex(base_p I) {
      Index = I;
    }

    base_p getIndex() const { return Index; }

    bool isStatic() const { return false;}

    DECLARE_VISIT;
};

/// \brief Stream with compile-time constant Index 
///
/// The index is not represented as ConstantVal object to simplify offset
/// analysis for stream optimization.
class StaticStreamIndex : public StreamIndex {
  private:
    int64_t Index;
  public:
    StaticStreamIndex(const std::string &Name, streamport_p Stream, int64_t Index, size_t W) : StreamIndex(Name, Stream), Index(Index) {
      setBitWidth(W);
    }

    StaticStreamIndex(const StaticStreamIndex&) = delete;
    StaticStreamIndex& operator=(const StaticStreamIndex&) = delete;

    virtual void setIndex(int64_t I) {
      Index = I;
    }
    int64_t getIndex() const { return Index; }

    bool isStatic() const {return true;}

    const std::string getUniqueName() const {
      return getName();
    }

    DECLARE_VISIT;
};

} // end ns oclacc

#endif /* STREAMS_H */
