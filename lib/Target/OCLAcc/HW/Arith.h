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
    Arith(const std::string &Name, unsigned BitWidth) : HW(Name, BitWidth) { }

    virtual const std::string getOp() {
      return "Arith";
    }

    DECLARE_VISIT;

};

class FPArith : public FPHW {
  public:
    FPArith (const std::string &Name, unsigned MantissaBitWidth, unsigned ExponentBitWidth) 
      : FPHW(Name, MantissaBitWidth, ExponentBitWidth) {
    }

    virtual const std::string getOp() {
      return "FPArith";
    }

    DECLARE_VISIT;
};

class Logic : public HW
{
  public:
    Logic(const std::string &Name) : HW(Name, 1)
    {
    }

    virtual const std::string getOp() {
      return "Logic";
    }

    DECLARE_VISIT;
};

class Add : public Arith
{
  public:
    Add(const std::string &Name, unsigned BitWidth) : Arith(Name, BitWidth)
    {
      //pass
    }
    virtual const std::string getOp() {
      return "Add";
    }
    DECLARE_VISIT;
};
class FAdd : public FPArith
{
  public:
    FAdd(const std::string &Name, unsigned MantissaBitWidth, unsigned ExponentBitWidth) : FPArith(Name, MantissaBitWidth, ExponentBitWidth)
    {
      //pass
    }
    virtual const std::string getOp() {
      return "FAdd";
    }
    DECLARE_VISIT;
};

class Sub : public Arith
{
  public:
    Sub(const std::string &Name, unsigned BitWidth) : Arith(Name, BitWidth)
    {
      //pass
    }
    virtual const std::string getOp() {
      return "Sub";
    }
    DECLARE_VISIT;
};
class FSub : public FPArith
{
  public:
    FSub(const std::string &Name, unsigned MantissaBitWidth, unsigned ExponentBitWidth) : FPArith(Name, MantissaBitWidth, ExponentBitWidth)
    {
      //pass
    }
    virtual const std::string getOp() {
      return "FSub";
    }
    DECLARE_VISIT;
};
class Mul : public Arith
{
  public:
    Mul(const std::string &Name, unsigned BitWidth) : Arith(Name, BitWidth)
    {
      //pass
    }
    virtual const std::string getOp() {
      return "Mul";
    }
    DECLARE_VISIT;
};
class FMul : public FPArith
{
  public:
    FMul(const std::string &Name, unsigned MantissaBitWidth, unsigned ExponentBitWidth) 
      : FPArith(Name, MantissaBitWidth, ExponentBitWidth)
    {
      //pass
    }
    virtual const std::string getOp() {
      return "FMul";
    }
    DECLARE_VISIT;
};

class UDiv : public Arith
{
  public:
    UDiv(const std::string &Name, unsigned BitWidth) : Arith(Name, BitWidth)
    {
      //pass
    }
    virtual const std::string getOp() {
      return "UDiv";
    }
    DECLARE_VISIT;
};
class SDiv : public Arith
{
  public:
    SDiv(const std::string &Name, unsigned BitWidth) : Arith(Name, BitWidth)
    {
      //pass
    }
    virtual const std::string getOp() {
      return "SDiv";
    }
    DECLARE_VISIT;
};
class FDiv : public FPArith
{
  public:
    FDiv(const std::string &Name, unsigned MantissaBitWidth, unsigned ExponentBitWidth) 
      : FPArith(Name, MantissaBitWidth, ExponentBitWidth)
    {
      //pass
    }
    virtual const std::string getOp() {
      return "FDiv";
    }
    DECLARE_VISIT;
};

class URem : public Arith
{
  public:
    URem(const std::string &Name, unsigned BitWidth) : Arith(Name, BitWidth)
    {
      //pass
    }
    virtual const std::string getOp() {
      return "URem";
    }
    DECLARE_VISIT;
};

class SRem : public Arith
{
  public:
    SRem(const std::string &Name, unsigned BitWidth) : Arith(Name, BitWidth)
    {
      //pass
    }
    virtual const std::string getOp() {
      return "SRam";
    }
    DECLARE_VISIT;
};

class FRem : public FPArith
{
  public:
    FRem(const std::string &Name, unsigned MantissaBitWidth, unsigned ExponentBitWidth) : FPArith(Name, MantissaBitWidth, ExponentBitWidth)
    {
      //pass
    }
    virtual const std::string getOp() {
      return "FRem";
    }
    DECLARE_VISIT;
};

class Shl : public Arith
{
  public:
    Shl(const std::string &Name, unsigned BitWidth) : Arith(Name, BitWidth)
    {
      //pass
    }
    virtual const std::string getOp() {
      return "Shl";
    }
    DECLARE_VISIT;
};
class LShr : public Arith
{
  public:
    LShr(const std::string &Name, unsigned BitWidth) : Arith(Name, BitWidth)
    {
      //pass
    }
    virtual const std::string getOp() {
      return "LShr";
    }
    DECLARE_VISIT;
};
class AShr : public Arith
{
  public:
    AShr(const std::string &Name, unsigned BitWidth) : Arith(Name, BitWidth)
    {
      //pass
    }
    virtual const std::string getOp() {
      return "AShr";
    }
    DECLARE_VISIT;
};
class And : public Arith
{
  public:
    And(const std::string &Name, unsigned BitWidth) : Arith(Name, BitWidth)
    {
      //pass
    }
    virtual const std::string getOp() {
      return "And";
    }
    DECLARE_VISIT;
};
class Or : public Arith
{
  public:
    Or(const std::string &Name, unsigned BitWidth) : Arith(Name, BitWidth)
    {
      //pass
    }
    virtual const std::string getOp() {
      return "Or";
    }
    DECLARE_VISIT;
};
class Xor : public Arith
{
  public:
    Xor(const std::string &Name, unsigned BitWidth) : Arith(Name, BitWidth)
    {
      //pass
    }
    virtual const std::string getOp() {
      return "Xor";
    }
    DECLARE_VISIT;
};

} //ns oclacc

#ifdef TYPENAME
#undef TYPENAME
#endif

#endif /* ARITH_H */
