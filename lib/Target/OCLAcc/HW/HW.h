#ifndef HW_H
#define HW_H

#include <vector>

#include "llvm/IR/Value.h"

#include "typedefs.h"
#include "Identifiable.h"
#include "Visitor/Base.h"

using namespace llvm;

namespace oclacc {

class HW : public Identifiable, public BaseVisitable
{
  protected:
    size_t Bitwidth;
    const llvm::Value *IR;
    block_p Block;

    std::vector<base_p> Ins;
    std::vector<base_p> Outs;

  public:
    HW(const std::string &Name, size_t Bitwidth, llvm::Value *IR=nullptr) : Identifiable(Name), Bitwidth(Bitwidth), IR(IR) { 
    }

    virtual ~HW() = default;

    NO_COPY_ASSIGN(HW)

    void setBlock(block_p P) {
      Block = P;
    }

    block_p getBlock(void) const {
      return Block;
    }

    virtual void appIn(base_p P) {
      if (P)
        Ins.push_back(P);
    }

    virtual void appOut(base_p P) {
      if (P)
        if (std::find(Outs.begin(), Outs.end(), P) == Outs.end())
          Outs.push_back(P);
    }

    virtual base_p getIn(size_t I) const { return I < Ins.size() ? Ins[I] : NULL;  }
    virtual base_p getOut(size_t I) const { return  I < Outs.size() ? Outs[I] : NULL; }

    virtual const std::vector<base_p> &getIns() const { return Ins; }
    virtual const std::vector<base_p> &getOuts() const { return Outs;  }

    virtual size_t getBitwidth() const { return Bitwidth; }
    virtual void setBitwidth(const size_t W) { Bitwidth=W; }

    const llvm::Value * getIR() const { return IR; }
    void setIR(const llvm::Value *P) { IR=P; }

    virtual const std::string dump(const std::string &Indent="") const;
};

/// \brief 
///   Make std::shared_pointer to HW object and set mapping to IR object
template<class HW, class ...Args> 
std::shared_ptr<HW> makeHW(const Value *IR, Args&& ...args) {
  std::shared_ptr<HW> P = std::make_shared<HW>(args...);
  P->setIR(IR);
  return P;
}

enum DataType {
  Float,
  Signed,
  Unsigned,
};

static const char * const Strings_DataType[] {
  "Float",  // 0
  "Signed",   // 1
  "Unsigned" // 2
};

#if 0
static class DataType 
{
};
static class Float : public DataType 
{
};
static class Signed : public DataType
{
  public:
};
static class Unsigned : public DataType 
{
  public:
};
#endif


} //ns oclacc

#endif /* HW_H */

