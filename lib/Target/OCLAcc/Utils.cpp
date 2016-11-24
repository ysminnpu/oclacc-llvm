#include <unistd.h>

#include "Utils.h"

#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/FileSystem.h"

using namespace llvm;

FileTy openFile(const std::string &N) {
  if (N.empty())
    llvm_unreachable("Filename empty");

  DEBUG_WITH_TYPE("FileIO", dbgs() << "Open File "+ N << "\n" );

  std::error_code EC;
  FileTy F = std::make_shared<raw_fd_ostream>(N, EC, sys::fs::F_RW | sys::fs::F_Text);
  if (EC) {
    errs() << "Failed to open logfile: " << EC.message() << '\n';
    llvm_unreachable("exit");
  }

  return F;
}

void changeDir(const std::string &D) {
  std::error_code EC;

  EC = sys::fs::create_directories(D);
  if (EC) {
    errs() << "Failed to create dir: " << EC.message() << '\n';
    llvm_unreachable("exit");
  }

  SmallVector<char, 1024> CurrentDir;
  EC = sys::fs::current_path(CurrentDir);
  if (EC) {
    errs() << "Failed to get current path: " << EC.message() << '\n';
    llvm_unreachable("exit");
  }

  if (chdir(D.c_str())) {
    errs() << "Failed to change to " << CurrentDir.data() << ": " << strerror(errno) << "\n";
    llvm_unreachable("exit");
  }

}
