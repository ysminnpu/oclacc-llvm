#ifndef CMP_H
#define CMP_H

#include "HW.h"

namespace oclacc {

class Compare : public HW {
  public:
    Compare(const std::string &Name) : HW(Name, 0) {
    }
    DECLARE_VISIT
};




}


#endif /* CMP_H */
