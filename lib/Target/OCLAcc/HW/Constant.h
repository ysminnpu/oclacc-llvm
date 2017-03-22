#ifndef CONST_H
#define CONST_H

#include "HW.h"


namespace oclacc {

class ConstVal : public HW
{
  public:
    const Datatype T;
    std::string Bits;

    ConstVal(const std::string &Name, const std::string Bits, size_t W);
    ConstVal(const std::string &Name, Datatype T, const std::string Bits, size_t W);

    const std::string dump(const std::string &Indent) const;

    const std::string getUniqueName() const;

    const std::string &getBits() const;

    DECLARE_VISIT

  private:
    virtual void addIn(base_p p) {};

};

}

#endif /* CONST_H */
