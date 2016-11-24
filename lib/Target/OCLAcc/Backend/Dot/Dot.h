#ifndef DOT_H
#define DOT_H

#include <sstream>
#include <memory>

#include "HW/Visitor/DFVisitor.h"
#include "HW/Writeable.h"
#include "HW/Identifiable.h"

#include "Utils.h"

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
    typedef DFVisitor super;

    FileTy FS;
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
