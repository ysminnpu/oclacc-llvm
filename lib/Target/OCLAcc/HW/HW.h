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

    inline void setParent(component_p P) {
      Parent = P;
    }

    inline component_p getParent(void) const {
      return Parent;
    }

    inline virtual void addIn(base_p P) {
      if (P)
        if (std::find(Ins.begin(), Ins.end(), P) == Ins.end())
          Ins.push_back(P);
    }

    inline virtual void addOut(base_p P) {
      if (P)
        if (std::find(Outs.begin(), Outs.end(), P) == Outs.end())
          Outs.push_back(P);
    }

    inline virtual void delIn(base_p P) {
      if (P) {
        HWListItTy I = std::find(Ins.begin(), Ins.end(), P);
        if (I != Ins.end())
          Ins.erase(I);
      }
    }

    inline virtual void delOut(base_p P) {
      if (P) {
        HWListItTy I = std::find(Outs.begin(), Outs.end(), P);
        if (I != Outs.end())
          Outs.erase(I);
      }
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
    
    inline virtual bool isFP() const {
      return false;
    }
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

    inline unsigned getMantissaBitWidth() const {
      return MantissaBitWidth;
    }
    inline unsigned getExponentBitWidth() const {
      return ExponentBitWidth;
    }

    inline virtual bool isFP() const {
      return true;
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

