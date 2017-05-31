#ifndef CONST_H
#define CONST_H

#include "HW.h"


namespace oclacc {

class ConstVal : public HW
{
  public:
    uint64_t Value;
    const Datatype T;
    std::string Bits;
    bool Static;

    ConstVal(uint64_t V);
    ConstVal(const std::string Name, const std::string Bits, size_t BitWidth);
    ConstVal(const std::string Name, Datatype T, const std::string Bits, size_t BitWidth);

    const std::string dump(const std::string &Indent="") const override;

    inline const std::string getUniqueName() const override {
      return getName();
    }

    inline const std::string &getBits() const {
      return Bits;
    }

    inline void setBits(const std::string &B) {
      Bits = B;
    }

    inline const std::string getBitString() const {
      return Bits;
    }

    inline void setValue(uint64_t V) {
      Value = V;
    }

    inline uint64_t getValue() {
      return Value;
    }

    inline bool isStatic() {
      return Static;
    }

    inline Datatype getDatatype() {
      return T;
    }

    DECLARE_VISIT

  private:
    using HW::addIn;
};

}

#endif /* CONST_H */
