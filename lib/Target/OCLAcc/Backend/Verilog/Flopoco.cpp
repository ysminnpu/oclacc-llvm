#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/FileSystem.h"

#include <string>
#include <sstream>
#include <regex>

#include "Macros.h"
#include "Flopoco.h"
#include "Utils.h"
#include "OperatorInstances.h"
#include "DesignFiles.h"

#define DEBUG_TYPE "flopoco"

using namespace oclacc;
using namespace llvm;
using namespace flopoco;

namespace flopoco {
// Map Name to ModuleInstantiation
ModMapTy ModuleMap;
} // end ns flopoco

/// \brief Generate modules
unsigned flopoco::genModule(const std::string Name, const std::string M, DesignFiles &Files) {

  // check if module already exists and return latency
  ModMapConstItTy MI = ModuleMap.find(Name);
  if (MI != ModuleMap.end()) return MI->second;


  char *P = std::getenv("FLOPOCO");
  if (!P)
    llvm_unreachable("$FLOPOCO not set");

  std::string Path(P);
 
  if (! sys::fs::can_execute(P))
    llvm_unreachable("Cannot execute $FLOPOCO");

  FileTy Log = openFile("flopoco.log");


  std::stringstream CS;
  CS << Path << " target=" << "Stratix5";
  CS << " frequency=200";
  CS << " plainVHDL=no";
  CS << " " << M;
  CS << " 2>&1";

  NDEBUG(CS.str());

  (*Log) << "[exec] " << CS.str() << "\n";

  std::array<char, 128> buffer;
  std::string Result;
  FILE* Pipe = popen(CS.str().c_str(), "r");

  if (!Pipe)
    llvm_unreachable("popen() $FLOPOCO failed");

  while (!feof(Pipe)) {
    if (fgets(buffer.data(), 128, Pipe) != NULL)
      Result += buffer.data();
  }


  if (int R = pclose(Pipe))
    llvm_unreachable("Returned nonzero");

  (*Log) << Result;
  (*Log) << "\n";
  Log->flush();


  // Extract FileName from Command; Pattern: outputFile=<name>
  std::regex RgxFileName("(?:outputFile=)\\S+(?= )");
  std::smatch FileNameMatch;
  if (!std::regex_search(M, FileNameMatch, RgxFileName))
    llvm_unreachable("Getting outputFile failed");

  std::string FileName = FileNameMatch[0];
  FileName = FileName.substr(FileName.find("=")+1);

  // Look for pipeline depth; Pattern: Entity: <name>
  std::regex RgxNoPipe("(?:\\n\\s+Not pipelined)(?=\\n)");
  std::regex RgxPipe("(?:\\n\\s+Pipeline depth = )\\d+(?=\\n)");

  std::smatch Match;

  unsigned Latency = 0;

  if (std::regex_search(Result, Match, RgxNoPipe)) {
    // pass
  } else if (std::regex_search(Result, Match, RgxPipe)) {
    std::smatch NumMatch;
    std::string Res = Match[0];
    std::regex_search(Res, NumMatch, std::regex("\\d+"));

    Latency = stoul(NumMatch[0]);

  } else
    llvm_unreachable("Invalid flopoco output");

  Files.addFile(FileName);

  Log->close();

  ModuleMap[Name] = Latency;

  NDEBUG("Latency of " << Name << ": " << Latency << " clock cycles");

  return Latency;
}

#ifdef DEBUG_TYPE
#undef DEBUG_TYPE
#endif
