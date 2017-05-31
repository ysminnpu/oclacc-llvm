#include <unistd.h>

#include "Utils.h"
#include "Macros.h"

#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/FileSystem.h"

#define DEBUG_TYPE "fileio"

using namespace llvm;

FileTy openFile(const std::string &N) {
  if (N.empty())
    report_fatal_error("No Filename");

  ODEBUG("Open File "+ N);

  std::error_code EC;
  FileTy F = std::make_shared<raw_fd_ostream>(N, EC, sys::fs::F_RW | sys::fs::F_Text);
  if (EC)
    report_fatal_error("Failed to open logfile: "+ EC.message());

  return F;
}

void changeDir(const std::string &D) {
  std::error_code EC;

  EC = sys::fs::create_directories(D);
  if (EC)
    report_fatal_error("Failed to create dir: "+ EC.message());

  SmallVector<char, 1024> CurrentDir;
  EC = sys::fs::current_path(CurrentDir);
  if (EC)
    report_fatal_error("Failed to get current path: "+ EC.message());

  if (chdir(D.c_str()))
    report_fatal_error("Failed to change to: "+ std::string(CurrentDir.data()) + strerror(errno));

}
