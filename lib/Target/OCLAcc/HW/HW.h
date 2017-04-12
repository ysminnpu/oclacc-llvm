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
  public:
    typedef std::vector<base_p> HWListTy;
    typedef HWListTy::iterator HWListItTy;
    typedef HWListTy::const_iterator HWListConstItTy;
    typedef HWListTy::size_type HWListSizeTy;

  protected:
    unsigned BitWidth;
    const llvm::Value *IR;
    component_p Parent;

    HWListTy Ins;
    HWListTy Outs;

  public:
    HW(const std::string &Name, unsigned BitWidth, llvm::Value *IR=nullptr) : Identifiable(Name), BitWidth(BitWidth), IR(IR) { 
      assert(BitWidth && "BitWidth must not be zero.");
    }

    virtual ~HW() = default;

    NO_COPY_ASSIGN(HW)

    void setParent(component_p P) {
      Parent = P;
    }

    component_p getParent(void) const {
      return Parent;
    }

    virtual void addIn(base_p P) {
      if (P)
        if (std::find(Ins.begin(), Ins.end(), P) == Ins.end())
          Ins.push_back(P);
    }

    virtual void addOut(base_p P) {
      if (P)
        if (std::find(Outs.begin(), Outs.end(), P) == Outs.end())
          Outs.push_back(P);
    }

    virtual base_p getIn(unsigned I) const { return I < Ins.size() ? Ins[I] : NULL;  }
    virtual base_p getOut(unsigned I) const { return  I < Outs.size() ? Outs[I] : NULL; }

    virtual const HWListTy &getIns() const { return Ins; }
    virtual const HWListTy &getOuts() const { return Outs;  }

    virtual unsigned getBitWidth() const { return BitWidth; }
    virtual void setBitWidth(unsigned W) { BitWidth=W; }

    const llvm::Value * getIR() const { return IR; }
    void setIR(const llvm::Value *P) { IR=P; }

    virtual const std::string dump(const std::string &Indent="") const;
};

class FPHW : public HW {
  private:
    unsigned MantissaBitWidth;
    unsigned ExponentBitWidth;

  public:
    FPHW (const std::string &Name, unsigned MantissaBitWidth, unsigned ExponentBitWidth) 
      : HW(Name, MantissaBitWidth + ExponentBitWidth + 1), 
        MantissaBitWidth(MantissaBitWidth),
        ExponentBitWidth(ExponentBitWidth) {
    }
    
    NO_COPY_ASSIGN(FPHW);

    unsigned getMantissaBitWidth() const {
      return MantissaBitWidth;
    }
    unsigned getExponentBitWidth() const {
      return ExponentBitWidth;
    }
};

enum Datatype {
  Undef,
  Half,
  Float,
  Double,
  Signed,
  Unsigned,
  Integer,
  Invalid
};

static const char * const Strings_Datatype[] {
  "Undef",
  "Half",
  "Float", 
  "Dounle",
  "Signed",
  "Unsigned",
  "Integer",
  "Invalid"
};


} //ns oclacc

#endif /* HW_H */

