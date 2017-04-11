#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/FileSystem.h"

#include <string>
#include <sstream>
#include <regex>

#include "Flopoco.h"
#include "Utils.h"
#include "OperatorInstances.h"
#include "DesignFiles.h"

#define DEBUG_TYPE "flopoco"

using namespace oclacc;
using namespace llvm;
using namespace flopoco;

// Flopoco functions
typedef std::map<std::string, std::string> ModMapTy;
typedef ModMapTy::const_iterator ModMapConstItTy;
typedef std::pair<std::string, std::string> ModMapElem;

// Map Name to ModuleInstantiation
ModMapTy flopoco::ModuleMap;
// Map HW.getUniqueName to Modulename
ModMapTy flopoco::NameHWMap;

/// \brief Return latency of module \param M reported by Flopoco
///
///
unsigned flopoco::getLatency(const std::string M) {
  FileTy Log = openFile(M+"log");

  return 0;
}

/// \brief Generate modules
unsigned flopoco::generateModules(OperatorInstances &Ops, DesignFiles &Files) {
  char *P = std::getenv("FLOPOCO");
  if (!P)
    llvm_unreachable("$FLOPOCO not set");

  std::string Path(P);
 
  if (! sys::fs::can_execute(P))
    llvm_unreachable("Cannot execute $FLOPOCO");

  FileTy Log = openFile("flopoco.log");

  for (const ModMapElem &M : ModuleMap) {

    // Look up HWName to create Operator
    const std::string Name = M.first;

    ModMapConstItTy NM = NameHWMap.find(Name);
    if (NM == NameHWMap.end())
      llvm_unreachable("No HW Name mapping");

    const std::string HWName = NM->second;
    

    std::stringstream CS;
    CS << Path << " target=" << "Stratix5";
    CS << " frequency=200";
    CS << " plainVHDL=no";
    CS << " " << M.second;
    CS << " 2>&1";

    DEBUG(dbgs() << "[exec] " << CS.str() << "\n");

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
    if (!std::regex_search(M.second, FileNameMatch, RgxFileName))
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

    Ops.addOperator(HWName, Name, Latency);

    Files.addFile(FileName);

    DEBUG(dbgs() << "Latency of " << Name << ": " << Latency << " clock cycles\n");
  }

  Log->close();

  return 0;
}

const std::string flopoco::printModules() {
  std::stringstream SS;
  for (const ModMapElem &S : ModuleMap) {
    SS << S.first << ":" << S.second << "\n";
  }

  return SS.str();
}

#ifdef DEBUG_TYPE
#undef DEBUG_TYPE
#endif
