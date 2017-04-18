#include <sstream>

#include "llvm/Support/Debug.h"
#include "llvm/Support/ErrorHandling.h"

#include "Utils.h"
#include "DesignFiles.h"

using namespace oclacc;

void DesignFiles::write(const std::string Filename) {
  std::stringstream DoS;
  FileTy DoFile = openFile(Filename);

  DoS << "# top level simulation\n";
  DoS << "# run with 'vsim -do " << Filename << "'\n";
  DoS << "vlib work\n";

  for (const std::string &S : Files) {
    if (ends_with(S, ".vhd")) {
      DoS << "vcom " << S << "\n";
    } else if (ends_with(S, ".v")) {
      DoS << "vlog -sv " << S << "\n";
    } else
      llvm_unreachable("Invalid filetype");
  }

  (*DoFile) << DoS.str();
  (*DoFile) << "quit\n";
  DoFile->close();
}
