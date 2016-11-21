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
  protected:
    Port(const std::string &Name, size_t W, const Datatype &T);

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
    ScalarPort(const std::string &Name, size_t W, const Datatype &T);

    bool isScalar();

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

  private:
    // must be a ordered list of indices to preserve correct store order.
    // Instead of a single list we use separate lists for the Index and the
    // access type.
    IndexListTy IndexList;
    AccessListTy AccessList;

  public:

    StreamPort(const std::string &Name, size_t W, ocl::AddressSpace, const Datatype &T);

    DECLARE_VISIT;

    bool isScalar();

    void addLoad(streamindex_p I);

    const IndexListTy get(AccessTy) const;

    const IndexListTy getLoads() const;

    const IndexListTy getStores() const;

    bool isLoad(streamindex_p) const;

    void addStore(streamindex_p);

    bool isStore(streamindex_p) const;

    const IndexListTy &getIndexList() const;

    const AccessListTy &getAccessList() const;
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
    StreamIndex(const std::string &Name, streamport_p Stream);

  public:
    StreamIndex(const StreamIndex&) = delete;
    StreamIndex& operator=(const StreamIndex&) = delete;

    streamport_p getStream() const;

    virtual bool isStatic() const = 0;

    DECLARE_VISIT
};


class DynamicStreamIndex : public StreamIndex {

  private:
    base_p Index;

  public:
    DynamicStreamIndex(const std::string &Name, streamport_p Stream, base_p Index);

    DynamicStreamIndex(const DynamicStreamIndex&) = delete;
    DynamicStreamIndex& operator=(const DynamicStreamIndex&) = delete;


    virtual void setIndex(base_p I);

    base_p getIndex() const;

    bool isStatic() const;

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
    StaticStreamIndex(const std::string &Name, streamport_p Stream, int64_t Index, size_t W);

    StaticStreamIndex(const StaticStreamIndex&) = delete;
    StaticStreamIndex& operator=(const StaticStreamIndex&) = delete;

    virtual void setIndex(int64_t I);
    int64_t getIndex() const;

    bool isStatic() const;

    const std::string getUniqueName() const;

    DECLARE_VISIT;
};

} // end ns oclacc

#endif /* PORT_H */
