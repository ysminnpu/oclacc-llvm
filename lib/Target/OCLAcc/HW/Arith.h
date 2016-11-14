#ifndef ARITH_H
#define ARITH_H

#include <algorithm>

#include "HW.h"

#include <cxxabi.h>
#define TYPENAME(x) abi::__cxa_demangle(typeid(x).name(),0,0,NULL)

namespace oclacc {

/// \brief Base class for all arithmetic operations
class Arith : public HW
{

  public:
    Arith(const std::string &Name, unsigned BitWidth=0) : HW(Name, BitWidth) { }

    virtual unsigned getMaxBitWidth() const {
      unsigned max = 0;
      for (auto &I : Ins) {
        max = std::max(max, I->getBitWidth() );
      }
      return max;
    }

    DECLARE_VISIT

};

class FPArith : public Arith {
  private:
    unsigned MantissaBitWidth;
    unsigned ExponentBitWidth;

  public:

    FPArith (const std::string &Name, unsigned MantissaBitWidth=0, unsigned ExponentBitWidth=0) 
      : Arith(Name, MantissaBitWidth + ExponentBitWidth), 
        MantissaBitWidth(MantissaBitWidth),
        ExponentBitWidth(ExponentBitWidth) {
    }

    unsigned getMantissaBitWidth() const {
      return MantissaBitWidth;
    }
    unsigned getExponentBitWidth() const {
      return ExponentBitWidth;
    }
};

class Logic : public HW
{
  public:
    Logic(const std::string &Name) : HW(Name, 1)
    {
    }

    DECLARE_VISIT
};

class Add : public Arith
{
  public:
    Add(const std::string &Name, unsigned BitWidth=0) : Arith(Name+TYPENAME(*this))
    {
      //pass
    }
    DECLARE_VISIT
};
class FAdd : public FPArith
{
  public:
    FAdd(const std::string &Name, unsigned MantissaBitWidth=0, unsigned ExponentBitWidth=0) : FPArith(Name+TYPENAME(*this), MantissaBitWidth, ExponentBitWidth)
    {
      //pass
    }
    DECLARE_VISIT
};

class Sub : public Arith
{
  public:
    Sub(const std::string &Name, unsigned BitWidth=0) : Arith(Name+TYPENAME(*this), BitWidth)
    {
      //pass
    }
    DECLARE_VISIT
};
class FSub : public FPArith
{
  public:
    FSub(const std::string &Name, unsigned MantissaBitWidth=0, unsigned ExponentBitWidth=0) : FPArith(Name+TYPENAME(*this), MantissaBitWidth, ExponentBitWidth)
    {
      //pass
    }
    DECLARE_VISIT
};
class Mul : public Arith
{
  public:
    Mul(const std::string &Name, unsigned BitWidth=0) : Arith(Name+TYPENAME(*this))
    {
      //pass
    }
    DECLARE_VISIT
};
class FMul : public FPArith
{
  public:
    FMul(const std::string &Name, unsigned BitWidth=0, unsigned MantissaBitWidth=0, unsigned ExponentBitWidth=0) 
      : FPArith(Name+TYPENAME(*this), MantissaBitWidth, ExponentBitWidth)
    {
      //pass
    }
    DECLARE_VISIT
};

class UDiv : public Arith
{
  public:
    UDiv(const std::string &Name, unsigned BitWidth=0) : Arith(Name+TYPENAME(*this))
    {
      //pass
    }
    DECLARE_VISIT
};
class SDiv : public Arith
{
  public:
    SDiv(const std::string &Name, unsigned BitWidth=0) : Arith(Name+TYPENAME(*this))
    {
      //pass
    }
    DECLARE_VISIT
};
class FDiv : public FPArith
{
  public:
    FDiv(const std::string &Name, unsigned MantissaBitWidth=0, unsigned ExponentBitWidth=0) 
      : FPArith(Name+TYPENAME(*this), MantissaBitWidth, ExponentBitWidth)
    {
      //pass
    }
    DECLARE_VISIT
};

class URem : public Arith
{
  public:
    URem(const std::string &Name, unsigned BitWidth=0) : Arith(Name+TYPENAME(*this))
    {
      //pass
    }
    DECLARE_VISIT
};

class SRem : public Arith
{
  public:
    SRem(const std::string &Name, unsigned BitWidth=0) : Arith(Name+TYPENAME(*this))
    {
      //pass
    }
    DECLARE_VISIT
};

class FRem : public FPArith
{
  public:
    FRem(const std::string &Name, unsigned MantissaBitWidth=0, unsigned ExponentBitWidth=0) : FPArith(Name+TYPENAME(*this), MantissaBitWidth, ExponentBitWidth)
    {
      //pass
    }
    DECLARE_VISIT
};

class Shl : public Arith
{
  public:
    Shl(const std::string &Name, unsigned BitWidth=0) : Arith(Name+TYPENAME(*this))
    {
      //pass
    }
    DECLARE_VISIT
};
class LShr : public Arith
{
  public:
    LShr(const std::string &Name, unsigned BitWidth=0) : Arith(Name+TYPENAME(*this))
    {
      //pass
    }
    DECLARE_VISIT
};
class AShr : public Arith
{
  public:
    AShr(const std::string &Name, unsigned BitWidth=0) : Arith(Name+TYPENAME(*this))
    {
      //pass
    }
    DECLARE_VISIT
};
class And : public Arith
{
  public:
    And(const std::string &Name, unsigned BitWidth=0) : Arith(Name+TYPENAME(*this))
    {
      //pass
    }
    DECLARE_VISIT
};
class Or : public Arith
{
  public:
    Or(const std::string &Name, unsigned BitWidth=0) : Arith(Name+TYPENAME(*this))
    {
      //pass
    }
    DECLARE_VISIT
};
class Xor : public Arith
{
  public:
    Xor(const std::string &Name, unsigned BitWidth=0) : Arith(Name+TYPENAME(*this))
    {
      //pass
    }
    DECLARE_VISIT
};

} //ns oclacc

#ifdef TYPENAME
#undef TYPENAME
#endif

#endif /* ARITH_H */
