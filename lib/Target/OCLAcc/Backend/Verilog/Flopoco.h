#ifndef FLOPOCO_H
#define FLOPOCO_H

#include <map>
#include <string>

namespace oclacc {
class BlockModule;
} // end ns oclacc

// Flopoco functions
namespace flopoco {

typedef std::map<std::string, unsigned> ModMapTy;
typedef ModMapTy::const_iterator ModMapConstItTy;
typedef std::pair<std::string, unsigned> ModMapElem;

/// \brief Generate a module
///
/// \param Name - UniqueName of the HW Object
/// \param M - Flopoco Module instantiation string
/// \param F - File collection
unsigned genModule(const std::string Name, const std::string M, oclacc::BlockModule &BM);

} // end ns flopoco


#endif /* FLOPOCO_H */
