#ifndef HW_H
#define HW_H

#include <vector>

#include "llvm/IR/Value.h"

#include "typedefs.h"
#include "Identifiable.h"
#include "Visitor/Visitable.h"

using namespace llvm;

namespace oclacc {

class HW : public Identifiable, public Visitable
{
  protected:
    size_t BitWidth;
    const llvm::Value *IR;
    block_p Block;

    std::vector<base_p> Ins;
    std::vector<base_p> Outs;

  public:
    HW(const std::string &Name, size_t BitWidth, llvm::Value *IR=nullptr) : Identifiable(Name), BitWidth(BitWidth), IR(IR) { 
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

    virtual size_t getBitWidth() const { return BitWidth; }
    virtual void setBitWidth(const size_t W) { BitWidth=W; }

    const llvm::Value * getIR() const { return IR; }
    void setIR(const llvm::Value *P) { IR=P; }

    virtual const std::string dump(const std::string &Indent="") const;
};

enum Datatype {
  Half,
  Float,
  Double,
  Signed,
  Unsigned,
  Invalid,
};

static const char * const Strings_Datatype[] {
  "Half",  // 0
  "Float",  // 0
  "Signed",   // 1
  "Unsigned" // 2
  "Invalid" // 2
};


} //ns oclacc

#endif /* HW_H */

