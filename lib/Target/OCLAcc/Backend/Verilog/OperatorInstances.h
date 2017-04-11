#ifndef OPERATORINSTANCES_H
#define OPERATORINSTANCES_H

#include <string>
#include <map>
#include <memory>
#include <unordered_set>

namespace oclacc {

struct Operator {
  const std::string Name;
  unsigned Cycles;

  Operator(const std::string Name, unsigned Cycles) : Name(Name), Cycles(Cycles) {
  }
};

typedef std::shared_ptr<Operator> op_p;

class OperatorInstances {
  public:
    typedef std::map<std::string, op_p > OpMapTy;
    typedef OpMapTy::const_iterator OpMapConstItTy;

    // Store Operators by name to speed up lookup
    typedef std::unordered_set<std::string> OpsTy;

  private:
    // Map HW.getUniqueName to Operator.Name
    OpMapTy HWOp;

    // Map Operator.Name to op_p
    OpMapTy NameMap;

    // Store all OpInstances with their name, e.g. fmul_8_23_10
    OpsTy Ops;

  public:
    /// \brief Add Operator. Creates a new if required or just adds another
    /// mapping for HWName
    void addOperator(const std::string HWName, const std::string OpName, unsigned Cycles);

    /// \brief Get existing Operator by Operator's name
    op_p getOperator(const std::string OpName);

    bool existsOperator(const std::string OpName);

    // Lookup HWNames
    bool existsOperatorForHW(const std::string HWName);
    op_p getOperatorForHW(const std::string HWName);
};

} // end ns oclacc

#endif /* OPERATORINSTANCES_H */
