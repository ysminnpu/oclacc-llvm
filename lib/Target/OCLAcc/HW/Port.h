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
    Port(const std::string &, unsigned, const Datatype &, bool);

  public:
    virtual bool isScalar() const = 0;

    inline Datatype getPortType() const {
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
    ScalarPort(const std::string &Name, unsigned W, const Datatype &T, bool Pipelined);

    inline bool isScalar() const override {
      return true;
    }

    DECLARE_VISIT;
};

class StreamAccess : public HW {
  private:
    streamindex_p Index;

    using HW::addIn;

  public:
    virtual bool isLoad() const = 0;
    virtual bool isStore() const = 0;

    streamport_p getStream() const;

    inline streamindex_p getIndex() const {
      return Index;
    }

  protected:
    StreamAccess(const std::string &Name, unsigned BitWidth, streamindex_p Index) : HW(Name, BitWidth), Index(Index) {
    }
};

class LoadAccess : public StreamAccess {
  private:
    using HW::addIn;
  public:
    LoadAccess(const std::string &Name, unsigned BitWidth, streamindex_p Index) : StreamAccess(Name, BitWidth, Index) {
    }

    inline virtual bool isLoad() const override {
      return true;
    }

    inline virtual bool isStore() const override {
      return false;
    }

    DECLARE_VISIT;
};

class StoreAccess : public StreamAccess {
  private:
    base_p Value;

    using HW::addOut;

  public:
    StoreAccess(const std::string &Name, unsigned BitWidth, streamindex_p Index, base_p Value) : StreamAccess(Name, BitWidth, Index), Value(Value) {
    }

    inline virtual bool isLoad() const override {
      return false;
    }

    inline virtual bool isStore() const override {
      return true;
    }

    inline base_p getValue() const {
      return Value;
    }

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
    typedef std::vector<streamaccess_p> AccessListTy;
    typedef std::vector<loadaccess_p> LoadListTy;
    typedef std::vector<storeaccess_p> StoreListTy;

  private:
    AccessListTy AccessList;

  public:
    StreamPort(const std::string &Name, unsigned BitWidth, ocl::AddressSpace, const Datatype &T);

    DECLARE_VISIT;

    inline bool isScalar() const override {
      return false;
    }

    inline const AccessListTy &getAccessList() const {
      return AccessList;
    }

    const AccessListTy getAccessList(block_p HWB) const;

    bool hasLoads() const;
    bool hasStores() const;

    inline const LoadListTy getLoads() const {
      LoadListTy L;
      for (const streamaccess_p S : AccessList) {
        if (S->isLoad()) L.push_back(std::static_pointer_cast<LoadAccess>(S));
      }
      return L;
    }

    inline void addAccess(streamaccess_p I) {
      AccessList.push_back(I);
    }

    inline const StoreListTy getStores() const {
      StoreListTy L;
      for (const streamaccess_p S : AccessList) {
        if (S->isStore()) L.push_back(std::static_pointer_cast<StoreAccess>(S));
      }
      return L;
    }

    const LoadListTy getStaticLoads() const;
    const LoadListTy getDynamicLoads() const;

    const StoreListTy getStaticStores() const;
    const StoreListTy getDynamicStores() const;

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


/// \brief DynamicStreamIndex results from a GEP instruction and can be used as
/// address operand for Loads and Stores to Streams.
class DynamicStreamIndex : public StreamIndex {

  public:
    DynamicStreamIndex(const std::string &Name, streamport_p, base_p, unsigned BitWidth);

    DynamicStreamIndex(const DynamicStreamIndex&) = delete;
    DynamicStreamIndex& operator=(const DynamicStreamIndex&) = delete;

    inline void setIndex(base_p I) {
      if (Ins.size() == 0)
        Ins.push_back(I);
      else
        Ins[0] = I;
    }

    inline base_p getIndex() const {
      assert(Ins.size() == 1 && "Invalid Ins for DynamicStreamIndex");
      return Ins[0];
    }

    inline bool isStatic() const override {
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
    StaticStreamIndex(const std::string &Name, streamport_p Stream, int64_t Index, unsigned BitWidth);

    StaticStreamIndex(const StaticStreamIndex&) = delete;
    StaticStreamIndex& operator=(const StaticStreamIndex&) = delete;

    virtual void setIndex(IndexTy I) {
      Index = I;
    }

    inline IndexTy getIndex() const {
      return Index;
    }

    inline bool isStatic() const override {
      return true;
    }

    inline const std::string getUniqueName() const override {
      return getName();
    }


    DECLARE_VISIT;
};

} // end ns oclacc

#endif /* PORT_H */
