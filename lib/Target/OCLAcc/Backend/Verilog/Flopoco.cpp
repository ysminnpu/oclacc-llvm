#include <string>
#include <sstream>
#include <regex>

#include "Macros.h"
#include "Flopoco.h"
#include "Utils.h"
#include "OperatorInstances.h"
#include "DesignFiles.h"
#include "VerilogModule.h"

#define DEBUG_TYPE "flopoco"

using namespace oclacc;
using namespace llvm;
using namespace flopoco;

namespace flopoco {
// Map Name to ModuleInstantiation
ModMapTy ModuleMap;
} // end ns flopoco

/// \brief Generate modules
unsigned flopoco::genModule(const std::string Name, const std::string M, BlockModule &BM) {

  // check if module already exists and return latency
  ModMapConstItTy MI = ModuleMap.find(Name);
  if (MI != ModuleMap.end()) return MI->second;

  std::string Path = getFPExPath("flopoco"); 

  std::stringstream CS;
  CS << Path << " target=" << "Stratix5";
  CS << " frequency=200";
  CS << " plainVHDL=no";
  CS << " " << M;
  CS << " 2>&1";

  ODEBUG(CS.str());

  const std::string Result = execute(CS.str());

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

  BM.addFile(FileName);

  ModuleMap[Name] = Latency;

  ODEBUG("Latency of " << Name << ": " << Latency << " clock cycles");

  return Latency;
}

std::string flopoco::convert(float V, unsigned MantissaBitWidth, unsigned ExponentBitwidth) {

  std::string Path = getFPExPath("fp2bin"); 

  std::stringstream CS;
  CS << Path << " " << MantissaBitWidth << " " << ExponentBitwidth << " " << V;

  ODEBUG(CS.str());

  std::string Result = execute(CS.str());

  if (Result[Result.length()-1] == '\n')
    Result = Result.erase(Result.length()-1);

  return Result;
}

#ifdef DEBUG_TYPE
#undef DEBUG_TYPE
#endif
