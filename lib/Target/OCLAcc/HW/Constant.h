#ifndef CONST_H
#define CONST_H

#include "HW.h"


namespace oclacc {

class ConstVal : public HW
{
  public:
    const Datatype T;
    std::string Bits;

    ConstVal(const std::string Name, const std::string Bits, size_t BitWidth);
    ConstVal(const std::string Name, Datatype T, const std::string Bits, size_t BitWidth);

    const std::string dump(const std::string &Indent="") const override;

    inline const std::string getUniqueName() const override {
      return getName();
    }

    inline const std::string &getBits() const {
      return Bits;
    }

    DECLARE_VISIT

  private:
    using HW::addIn;
};

}

#endif /* CONST_H */
