#ifndef VERILOG_H
#define VERILOG_H

#include <sstream>

#include "HW/Visitor/DFVisitor.h"
#include "HW/Writeable.h"
#include "HW/Identifiable.h"

namespace oclacc {

class DesignUnit;
class Kernel;
class Block;
class ScalarPort;
class StreamPort;
class Arith;
class FPArith;

class Verilog : public DFVisitor {
  private:
    typedef std::shared_ptr<llvm::raw_fd_ostream> fs_t;
  public:
    Verilog();
    ~Verilog();

    int visit(DesignUnit &R);
    int visit(Kernel &R);
    int visit(Block &R);
    int visit(ScalarPort &R);
    int visit(StreamPort &R);
    int visit(Arith &R);
    int visit(FPArith &R);

    fs_t  openFile(const std::string &N) {
      if (N.empty())
        llvm_unreachable("Filename empty");

      DEBUG_WITH_TYPE("FileIO", llvm::dbgs() << "Open File "+ N << "\n" );

      std::error_code EC;
      fs_t F = std::make_shared<llvm::raw_fd_ostream>(N, EC, llvm::sys::fs::F_RW | llvm::sys::fs::F_Text);
      if (EC)
        llvm_unreachable("File could not be opened.");

      return F;
    }

    void closeFile(fs_t F) {
      F->close();
    }
};

} // end ns oclacc


#endif /* VERILOG_H */
