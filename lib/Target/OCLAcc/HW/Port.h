#ifndef PORT_H
#define PORT_H

#include <vector>
#include <iterator>

#include "HW.h"

#include "Visitor/Visitable.h"
#include "typedefs.h"
#include "Identifiable.h"
#include "OpenCLDefines.h"

namespace oclacc {

class Port : public HW {
  private:
    const Datatype PortType;
    
  protected:
    bool Pipelined = false;


  protected:
    Port(const std::string &, size_t, const Datatype &, bool);

  public:
    virtual bool isScalar() const=0;

    inline const Datatype getPortType() const {
      return PortType;
    }

    inline bool isPipelined(void) const {
      return Pipelined;
    }

    inline bool isFP() const {
      return PortType == Half || PortType == Float || PortType == Double;
    }

};

/// \brief Input or Output Port
///
/// It depends on the Block, whether a Port is input or output. ScalarPorts,
/// when used as input, may contain a list of Conditions.
///
/// ScalarPorts are \param Pipelined when they are promoted Kernel Arguments or
/// Values defined in BBs. Normal Arguments may not change between WorkItems, so
/// there is no need to propagate them between BBs.
///
class ScalarPort : public Port {
  public:
    ScalarPort(const std::string &Name, size_t W, const Datatype &T, bool Pipelined);

    bool isScalar() const;

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
    enum AccessTy {
      Invalid,
      Load,
      Store
    };

    typedef std::vector<streamindex_p> IndexListTy;
    typedef IndexListTy::iterator IndexListIt;
    typedef IndexListTy::const_iterator IndexListConstIt;
    typedef std::vector<AccessTy> AccessListTy;

    typedef std::vector<staticstreamindex_p> StaticIndexListTy;
    typedef StaticIndexListTy::iterator StaticIndexListIt;
    typedef StaticIndexListTy::const_iterator StaticIndexListConstIt;

    typedef std::vector<dynamicstreamindex_p> DynamicIndexListTy;
    typedef DynamicIndexListTy::iterator DynamicIndexListIt;
    typedef DynamicIndexListTy::const_iterator DynamicIndexListConstIt;
  private:
    // must be a ordered list of indices to preserve correct store order.
    // Instead of a single list we use separate lists for the Index and the
    // access type.
    IndexListTy IndexList;

    // We store the type of access in a separate list. IndexList[i] has an
    // access ty of AccessList[i].
    AccessListTy AccessList;

  public:
    StreamPort(const std::string &Name, size_t W, ocl::AddressSpace, const Datatype &T);

    DECLARE_VISIT;

    inline bool isScalar() const {
      return false;
    }

    // Combined
    const IndexListTy get(AccessTy) const;
    const StaticIndexListTy getStaticIndizes() const;
    const DynamicIndexListTy getDynamicIndizes() const;

    inline const IndexListTy &getIndexList() const {
      return IndexList;
    }

    inline const AccessListTy &getAccessList() const {
      return AccessList;
    }


    bool hasLoads() const;
    bool hasStores() const;

    // Load methods
    //
    bool isLoad(StreamIndex *) const;
    bool isLoad(streamindex_p P) const { return isLoad(P.get()); } 

    inline const IndexListTy getLoads() const {
      return get(Load);
    }

    const StaticIndexListTy getStaticLoads() const;
    const DynamicIndexListTy getDynamicLoads() const;

    void addLoad(streamindex_p I);

    // Store methods
    //
    bool isStore(StreamIndex *) const;
    bool isStore(streamindex_p P) const { return isStore(P.get()); }

    inline const IndexListTy getStores() const {
      return get(Store);
    }

    const StaticIndexListTy getStaticStores() const;
    const DynamicIndexListTy getDynamicStores() const;

    void addStore(streamindex_p);

};

/// \brief StreamIndex
///
/// In and Out used for data while the index depends on the actual subcalss
/// type.
class StreamIndex : public HW {
  private:
    streamport_p Stream;

  protected:
    StreamIndex(const std::string &Name, streamport_p Stream, unsigned BitWidth);

  public:
    StreamIndex(const StreamIndex&) = delete;
    StreamIndex& operator=(const StreamIndex&) = delete;

    inline const streamport_p getStream() const {
      return Stream;
    }

    inline unsigned getStreamBitWidth() const {
      return Stream->getBitWidth();
    }

    virtual bool isStatic() const = 0;

    DECLARE_VISIT
};


class DynamicStreamIndex : public StreamIndex {

  private:
    base_p Index;

  public:
    DynamicStreamIndex(const std::string &Name, streamport_p, base_p, unsigned BitWidth);

    DynamicStreamIndex(const DynamicStreamIndex&) = delete;
    DynamicStreamIndex& operator=(const DynamicStreamIndex&) = delete;

    void setIndex(base_p I) {
      Index = I;
    }

    base_p getIndex() const {
      return Index;
    }

    bool isStatic() const {
      return false;
    }

    DECLARE_VISIT;
};

/// \brief Stream with compile-time constant Index 
///
/// The index is not represented as ConstantVal object to simplify offset
/// analysis for stream optimization.
class StaticStreamIndex : public StreamIndex {
  public:
    typedef int64_t IndexTy;
  private:
    IndexTy Index;
  public:
    StaticStreamIndex(const std::string &Name, streamport_p Stream, int64_t Index, size_t BitWidth);

    StaticStreamIndex(const StaticStreamIndex&) = delete;
    StaticStreamIndex& operator=(const StaticStreamIndex&) = delete;

    virtual void setIndex(IndexTy I) {
      Index = I;
    }

    inline IndexTy getIndex() const {
      return Index;
    }

    inline bool isStatic() const {
      return true;
    }

    inline const std::string getUniqueName() const {
      return getName();
    }


    DECLARE_VISIT;
};

} // end ns oclacc

#endif /* PORT_H */
