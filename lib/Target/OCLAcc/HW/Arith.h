#ifndef ARITH_H
#define ARITH_H

#include <algorithm>

#include "HW.h"

namespace oclacc {

/// \brief Base class for all arithmetic operations
class Arith : public HW
{

  public:
    Arith(const std::string &Name, size_t Bitwidth=0) : HW(Name, Bitwidth) { }

    virtual size_t getMaxBitwidth() const {
      size_t max = 0;
      for (auto &I : Ins) {
        max = std::max(max, I->getBitwidth() );
      }
      return max;
    }

    DECLARE_VISIT

};

class FPArith : public Arith {
  private:
    size_t MantissaBitwidth;
    size_t ExponentBitwidth;

  public:

    FPArith (const std::string &Name, size_t MantissaBitwidth=0, size_t ExponentBitwidth=0) 
      : Arith(Name, MantissaBitwidth + ExponentBitwidth), 
        MantissaBitwidth(MantissaBitwidth),
        ExponentBitwidth(ExponentBitwidth) {
    }

    size_t getMantissaBitwidth() const {
      return MantissaBitwidth;
    }
    size_t getExponentBitwidth() const {
      return ExponentBitwidth;
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
    Add(const std::string &Name, size_t Bitwidth=0) : Arith(Name)
    {
      //pass
    }
    DECLARE_VISIT
};
class FAdd : public FPArith
{
  public:
    FAdd(const std::string &Name, size_t MantissaBitwidth=0, size_t ExponentBitwidth=0) : FPArith(Name, MantissaBitwidth, ExponentBitwidth)
    {
      //pass
    }
    DECLARE_VISIT
};

class Sub : public Arith
{
  public:
    Sub(const std::string &Name, size_t Bitwidth=0) : Arith(Name, Bitwidth)
    {
      //pass
    }
    DECLARE_VISIT
};
class FSub : public FPArith
{
  public:
    FSub(const std::string &Name, size_t MantissaBitwidth=0, size_t ExponentBitwidth=0) : FPArith(Name, MantissaBitwidth, ExponentBitwidth)
    {
      //pass
    }
    DECLARE_VISIT
};
class Mul : public Arith
{
  public:
    Mul(const std::string &Name, size_t Bitwidth=0) : Arith(Name)
    {
      //pass
    }
    DECLARE_VISIT
};
class FMul : public FPArith
{
  public:
    FMul(const std::string &Name, size_t Bitwidth=0, size_t MantissaBitwidth=0, size_t ExponentBitwidth=0) 
      : FPArith(Name, MantissaBitwidth, ExponentBitwidth)
    {
      //pass
    }
    DECLARE_VISIT
};

class UDiv : public Arith
{
  public:
    UDiv(const std::string &Name, size_t Bitwidth=0) : Arith(Name)
    {
      //pass
    }
    DECLARE_VISIT
};
class SDiv : public Arith
{
  public:
    SDiv(const std::string &Name, size_t Bitwidth=0) : Arith(Name)
    {
      //pass
    }
    DECLARE_VISIT
};
class FDiv : public FPArith
{
  public:
    FDiv(const std::string &Name, size_t MantissaBitwidth=0, size_t ExponentBitwidth=0) 
      : FPArith(Name, MantissaBitwidth, ExponentBitwidth)
    {
      //pass
    }
    DECLARE_VISIT
};

class URem : public Arith
{
  public:
    URem(const std::string &Name, size_t Bitwidth=0) : Arith(Name)
    {
      //pass
    }
    DECLARE_VISIT
};

class SRem : public Arith
{
  public:
    SRem(const std::string &Name, size_t Bitwidth=0) : Arith(Name)
    {
      //pass
    }
    DECLARE_VISIT
};

class FRem : public FPArith
{
  public:
    FRem(const std::string &Name, size_t MantissaBitwidth=0, size_t ExponentBitwidth=0) : FPArith(Name, MantissaBitwidth, ExponentBitwidth)
    {
      //pass
    }
    DECLARE_VISIT
};

class Shl : public Arith
{
  public:
    Shl(const std::string &Name, size_t Bitwidth=0) : Arith(Name)
    {
      //pass
    }
    DECLARE_VISIT
};
class LShr : public Arith
{
  public:
    LShr(const std::string &Name, size_t Bitwidth=0) : Arith(Name)
    {
      //pass
    }
    DECLARE_VISIT
};
class AShr : public Arith
{
  public:
    AShr(const std::string &Name, size_t Bitwidth=0) : Arith(Name)
    {
      //pass
    }
    DECLARE_VISIT
};
class And : public Arith
{
  public:
    And(const std::string &Name, size_t Bitwidth=0) : Arith(Name)
    {
      //pass
    }
    DECLARE_VISIT
};
class Or : public Arith
{
  public:
    Or(const std::string &Name, size_t Bitwidth=0) : Arith(Name)
    {
      //pass
    }
    DECLARE_VISIT
};
class Xor : public Arith
{
  public:
    Xor(const std::string &Name, size_t Bitwidth=0) : Arith(Name)
    {
      //pass
    }
    DECLARE_VISIT
};

} //ns oclacc

#endif /* ARITH_H */
