#ifndef FLOPOCO_H
#define FLOPOCO_H

#include <map>
#include <string>

#include "Utils.h"

#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/FileSystem.h"

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

std::string convert(float V, unsigned MantissaBitWidth, unsigned ExponentBitwidth);

inline std::string getFPExPath(const std::string &E) {
  char *P = std::getenv("FLOPOCO_PATH");
  if (!P) {
    llvm::errs() << "$FLOPOCO_PATH not set\n";
    exit(1);
  }

  std::string Path(P);
  if (Path[Path.length()-1] != '/')
    Path += '/';

  Path += E;
 
  if (! llvm::sys::fs::can_execute(Path)) {
    llvm::errs() << "Cannot execute " << Path << "\n";
    exit(1);
  }

  return Path;
}

inline std::string execute(const std::string &C) {
  FileTy Log = openFile("flopoco.log");
  (*Log) << "[exec] " << C << "\n";

  std::array<char, 128> buffer;
  std::string Result;
  FILE* Pipe = popen(C.c_str(), "r");

  if (!Pipe) {
    llvm::errs() << "Failed to execute " << C << "\n";
    exit(1);
  }

  while (!feof(Pipe)) {
    if (fgets(buffer.data(), 128, Pipe) != NULL)
      Result += buffer.data();
  }

  if (int R = pclose(Pipe)) {
    llvm::errs() << "Failed to close pipe " << C << "\n";
    exit(1);
  }

  (*Log) << Result;
  (*Log) << "\n";
  Log->flush();

  return Result;
}

} // end ns flopoco

#endif /* FLOPOCO_H */
