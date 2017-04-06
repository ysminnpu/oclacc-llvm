#ifndef CMP_H
#define CMP_H

#include "llvm/IR/InstrTypes.h"

#include "HW.h"

namespace oclacc {

class Compare : public HW {

  public:
    typedef llvm::CmpInst::Predicate PredTy;

  private:
    PredTy Pred;

  public:
    Compare(const std::string &Name, Compare::PredTy P) : HW(Name, 1), Pred(P) {
    }

    inline const std::string getPredAsString(PredTy P) {
      switch (P) {
        case  0: return "FCMP_FALSE";
        case  1: return "FCMP_OEQ";
        case  2: return "FCMP_OGT";
        case  3: return "FCMP_OGE";
        case  4: return "FCMP_OLT";
        case  5: return "FCMP_OLE";
        case  6: return "FCMP_ONE";
        case  7: return "FCMP_ORD";
        case  8: return "FCMP_UNO";
        case  9: return "FCMP_UEQ";
        case 10: return "FCMP_UGT";
        case 11: return "FCMP_UGE";
        case 12: return "FCMP_ULT";
        case 13: return "FCMP_ULE";
        case 14: return "FCMP_UNE";
        case 15: return "FCMP_TRUE";

        case 32: return "ICMP_EQ";
        case 33: return "ICMP_NE";
        case 34: return "ICMP_UGT";
        case 35: return "ICMP_UGE";
        case 36: return "ICMP_ULT";
        case 37: return "ICMP_ULE";
        case 38: return "ICMP_SGT";
        case 39: return "ICMP_SGE";
        case 40: return "ICMP_SLT";
        case 41: return "ICMP_SLE";
        default: return "INV_CMP";
      }
    }

    inline const std::string getOp() {
      return getPredAsString(Pred);
    }

    inline PredTy getPred() const {
      return Pred;
    }

    DECLARE_VISIT;
};

class IntCompare : public Compare {
  public:
    IntCompare(const std::string &Name, Compare::PredTy P) : Compare(Name, P) {
    }
    DECLARE_VISIT;
};

class FPCompare : public Compare {
  public:
    FPCompare(const std::string &Name, Compare::PredTy P) : Compare(Name, P) {
    }
    DECLARE_VISIT;
};

}


#endif /* CMP_H */
