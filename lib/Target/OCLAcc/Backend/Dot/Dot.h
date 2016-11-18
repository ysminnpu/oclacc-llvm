#ifndef DOT_H
#define DOT_H

#include <sstream>
#include <memory>

#include "HW/Visitor/DFVisitor.h"
#include "HW/Writeable.h"
#include "HW/Identifiable.h"

namespace oclacc {

class DesignUnit;
class Kernel;
class Block;
class Arith;
class FPArith;
class Compare;
class ConstVal;
class ScalarPort;
class StreamPort;

class Dot: public DFVisitor {
  private:
    typedef std::unique_ptr<llvm::raw_fd_ostream> fs_t;
    typedef DFVisitor super;

    fs_t FS;
    unsigned IndentLevel;

    std::stringstream Connections;

    const std::string nodeOutStream(const std::string &Label) {
      return "[shape=invhouse,fillcolor=\"/purples9/5\",style=filled,tailport=n,label=\"" + Label + "\"]";
    }
    const std::string nodeInStream(const std::string &Label) {
      return "[shape=house,fillcolor=\"/purples9/5\",style=filled,tailport=n,label=\"" + Label + "\"]";
    }

    const std::string nodeInStreamIndex(const std::string &Label) { 
      return "[shape=rarrow,fillcolor=\"/purples9/5\",style=filled,tailport=n,label=\"" + Label + "\"]";
    }

    const std::string nodeOutStreamIndex(const std::string &Label) { 
      return "[shape=larrow,fillcolor=\"/purples9/5\",style=filled,tailport=n,label=\"" + Label + "\"]";
    }

    const std::string Indent() {
      return std::string(IndentLevel*2, ' ');
    }

    llvm::raw_fd_ostream &F() {
      (*FS) << Indent();
      return *FS;
    }

    std::stringstream &Conn() {
      Connections << "    ";
      return Connections;
    }

  public:
    Dot();
    ~Dot();

    fs_t  openFile(const std::string &N) {
      if (N.empty())
        llvm_unreachable("Filename empty");

      DEBUG_WITH_TYPE("FileIO", llvm::dbgs() << "Open File "+ N << "\n" );

      std::error_code EC;
      fs_t F = std::make_unique<llvm::raw_fd_ostream>(N, EC, llvm::sys::fs::F_RW | llvm::sys::fs::F_Text);
      if (EC)
        llvm_unreachable("File could not be opened.");

      return F;
    }

    void closeFile(fs_t F) {
      F->close();
    }

    // visit methods
    virtual int visit(DesignUnit &);
    virtual int visit(Kernel &);
    virtual int visit(Block &);
    virtual int visit(Arith &);
    virtual int visit(FPArith &);
    virtual int visit(Compare &);
    virtual int visit(ConstVal &);
    virtual int visit(ScalarPort &);
    virtual int visit(StreamPort &);
    virtual int visit(Mux &);
};

} // end ns oclacc


#endif /* DOT_H */
