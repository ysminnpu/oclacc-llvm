#ifndef CONST_H
#define CONST_H

#include "HW.h"

namespace oclacc {

class ConstVal : public HW
{
  public:
    const DataType T;
    const uint64_t V;

    ConstVal(const std::string &Name, uint64_t V, size_t W);
    ConstVal(const std::string &Name, DataType T, uint64_t V, size_t W);

    const std::string dump(const std::string &Indent) const;

    const std::string getUniqueName() const;

    DECLARE_VISIT

  private:
    virtual void appIn(base_p p) {};

};

}

#endif /* CONST_H */
