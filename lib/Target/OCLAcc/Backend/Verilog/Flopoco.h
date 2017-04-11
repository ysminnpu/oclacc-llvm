#ifndef FLOPOCO_H
#define FLOPOCO_H

#include <map>
#include <string>

namespace oclacc {
class OperatorInstances;
class DesignFiles;
} // end ns oclacc

// Flopoco functions
namespace flopoco {

typedef std::map<std::string, std::string> ModMapTy;
typedef ModMapTy::const_iterator ModMapConstItTy;
typedef std::pair<std::string, std::string> ModMapElem;

// Map Name to ModuleInstantiation
extern ModMapTy ModuleMap;
// Map HW.getUniqueName to Modulename
extern ModMapTy NameHWMap;

/// \brief Return latency of module \param M reported by Flopoco
///
///
unsigned getLatency(const std::string M);

/// \brief Generate modules
unsigned generateModules(oclacc::OperatorInstances &, oclacc::DesignFiles &);

const std::string printModules();

/// \brief Add module to global module list
///
/// \param HWName - UniqueName of the HW Object
/// \param Name - Operator Name. Shared between multiple Instances
/// \param M - Flopoco Module instantiation string
inline void addModule(const std::string HWName, const std::string Name, const std::string M) {
  ModuleMap[Name] = M;
  NameHWMap[Name] = HWName;
}

} // end ns flopoco


#endif /* FLOPOCO_H */
