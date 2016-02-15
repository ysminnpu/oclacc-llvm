#ifndef VHDLGEN_H
#define VHDLGEN_H

#include <string>
#include <sstream>
#include <map>
#include <system_error>

#include "llvm/Support/Debug.h"
#include "../../todo.h"

#include "../Identifiable.h"
#include "../Writeable.h"

namespace oclacc {

class VhdlPort;
typedef std::shared_ptr<VhdlPort> port_p;

class VhdlSignal;
typedef std::shared_ptr<VhdlSignal> signal_p;

class VhdlSignal : public Identifiable {
  public:
    VhdlSignal(const std::string &Name, size_t W) : Identifiable(Name), W(W) {
    }

    VhdlSignal( const VhdlSignal & ) = delete;
    VhdlSignal &operator =(const VhdlPort &) = delete;

    size_t W;
};

class VhdlPort : public VhdlSignal {
  public:

    VhdlPort(const std::string &Name, size_t W) : VhdlSignal(Name, W) {
    }
    VhdlPort( const VhdlPort & ) = delete;
    VhdlPort &operator =(const VhdlPort &) = delete;

    virtual const std::string getPortDecl(const std::string &Indent = "") = 0;
};

class VhdlInPort : public VhdlPort {
  public:
    VhdlInPort(const std::string &Name, size_t W=1) : VhdlPort( Name, W ) {
    }

    VhdlInPort( const VhdlInPort & ) = delete;
    VhdlInPort &operator =(const VhdlInPort &) = delete;

    const std::string getPortDecl(const std::string &Indent = "") {
      if (W == 1)
        return Name + " : in std_logic";
      else {
        std::stringstream SS;
        SS << Indent << Name << " : in std_logic_vector(" << W-1 << " downto 0)";
        return SS.str();
      }
    }
};

class VhdlOutPort : public VhdlPort {
  public:
    VhdlOutPort(const std::string &Name, size_t W=1) : VhdlPort( Name, W ) {
    }

    VhdlOutPort( const VhdlOutPort & ) = delete;
    VhdlOutPort &operator =(const VhdlOutPort &) = delete;

    const std::string getPortDecl(const std::string &Indent = "") {
      if (W == 1)
        return Name + " : out std_logic";
      else {
        std::stringstream SS;
        SS << Name << " : out std_logic_vector(" << W-1 << " downto 0)";
        return SS.str();
      }
    }
};

class VhdlMemPort : public VhdlPort {
  protected:
    signal_p Data;
    signal_p Addr;
    signal_p Valid;
    signal_p Ack;
  public:
    VhdlMemPort(const std::string &Name, size_t W=1) : VhdlPort("mem_"+Name, W) {
      Data = std::make_shared<VhdlSignal>(this->Name+"_data", W);
      Addr = std::make_shared<VhdlSignal>(this->Name+"_addr", 64);
      Valid = std::make_shared<VhdlSignal>(this->Name+"_valid", 1);
      Ack = std::make_shared<VhdlSignal>(this->Name+"_ack", 1);
    }
    virtual ~VhdlMemPort() {};

    VhdlMemPort( const VhdlMemPort & ) = delete;
    VhdlMemPort &operator =(const VhdlMemPort &) = delete;

    virtual const std::string getPortDecl(const std::string &Indent) = 0;

    signal_p getData() {return Data;}
    signal_p getAddr() {return Addr;}
    signal_p getValid() {return Valid;}
    signal_p getAck() {return Ack;}
};

class VhdlMemInPort : public VhdlMemPort {
  public:
    VhdlMemInPort(const std::string &Name, size_t W=1) : VhdlMemPort(Name, W) {}

    VhdlMemInPort( const VhdlMemInPort & ) = delete;
    VhdlMemInPort &operator =(const VhdlMemInPort &) = delete;

    const std::string getPortDecl(const std::string &Indent = "") {
      std::stringstream SS;

      SS << Indent << Data->getName() << " : in std_logic_vector(" << Data->W-1 << " downto 0);\n";
      SS << Indent << Addr->getName() << " : out std_logic_vector(" << Addr->W-1 << " downto 0);\n";
      SS << Indent << Valid->getName() << " : in std_logic;\n";
      SS << Indent << Ack->getName() << " : out std_logic";

      return SS.str();
    }
};

class VhdlMemOutPort : public VhdlMemPort {
  public:
    VhdlMemOutPort(const std::string &Name, size_t W=1) : VhdlMemPort(Name, W) { }

    VhdlMemOutPort( const VhdlMemOutPort & ) = delete;
    VhdlMemOutPort &operator =(const VhdlMemOutPort &) = delete;

    const std::string getPortDecl(const std::string &Indent = "") {
      std::stringstream SS;

      SS << Indent << Data->getName() << " : out std_logic_vector(" << Data->W-1 << " downto 0);\n";
      SS << Indent << Addr->getName() << " : out std_logic_vector(" << Addr->W-1 << " downto 0);\n";
      SS << Indent << Valid->getName() << " : out std_logic;\n";
      SS << Indent << Ack->getName() << " : in std_logic";

      return SS.str();
    }
};


/* 
 * Maintenance Ports
 */
struct VhdlReset: VhdlPort {
  VhdlReset( ) : VhdlPort( "DefRst", 1 ) {
  }
  VhdlReset( const std::string &Name) : VhdlPort( Name, 1 ) {
  }
};
struct VhdlClk : VhdlPort {
  VhdlClk() : VhdlPort( "DefClk" , 1 ) {
  }

  VhdlClk( const std::string &Name) : VhdlPort( Name, 1 ) {
  }
};

struct VhdlInOutPort : VhdlPort {
  VhdlInOutPort( const std::string &Name, size_t W) : VhdlPort( Name, W ) {
  }
};


class VhdlWriteable : public Writeable {
  protected:
    VhdlWriteable ( const std::string &FileName ) : Writeable(FileName+".vhdl") { }

  public:
    VhdlWriteable (const VhdlWriteable &) = delete;
    VhdlWriteable &operator =(const VhdlWriteable &) = delete;
};

class VhdlCombinational {
  public:
    virtual const std::string write(const std::string &Indent) = 0;

    virtual ~VhdlCombinational() { }
};

class VhdlEntity : public Identifiable, public VhdlWriteable {

  public:
    std::vector< port_p > InScalar;
    std::vector< port_p > InMem;
    std::vector< port_p > OutScalar;
    std::vector< port_p > OutMem;
    std::vector< VhdlSignal > Const;

    port_p Clk;
    port_p Reset;

    std::vector< signal_p > Signals;
    std::vector< std::shared_ptr<VhdlEntity> > Components;
    std::vector< std::shared_ptr<VhdlCombinational> > Combinational;

    //To, From
    std::map<signal_p, signal_p > SignalAssignments;
    std::map<signal_p, size_t > ValueAssignments;

    /* 
     * Ctors, Operators
     */
    VhdlEntity( const std::string &Name,
        port_p Clk,
        port_p Reset );

    VhdlEntity (const VhdlEntity &) = delete;
    VhdlEntity &operator =(const VhdlEntity &) = delete;

    /*
     * Methods
     */

    port_p getClk() const {
      return Clk;
    }

    port_p getReset() const {
      return Reset;
    }

    virtual void addSignal(const signal_p S) {
      Signals.push_back(S);
    }

    virtual void addComponent(const std::shared_ptr<VhdlEntity> C) {
      Components.push_back(C);
    }
    virtual const std::vector< std::shared_ptr<VhdlEntity> > & getComponents() const {
      return Components;
    }

    virtual void addCombinational(const std::shared_ptr<VhdlCombinational> C) {
      Combinational.push_back(C);
    }
    virtual void addSignalAssignment(signal_p T, signal_p F) {
      SignalAssignments[T] = F;
    }

    virtual void addValueAssignment(signal_p T, size_t V) {
      ValueAssignments[T] = V;
    }

    virtual void writeArch() const {
      //DEBUG( llvm::dbgs() << __PRETTY_FUNCTION__ << "\n" );
    };

    void write(const std::string &Indent="") {
      DEBUG_WITH_TYPE("VhdlVisitor", llvm::dbgs() << "Write Contents "+ FileName << "\n" );

      // sort
      std::sort(Signals.begin(), Signals.end(),
          [](const signal_p &a, const signal_p &b) -> bool {
            return (a->getName().compare(b->getName()) < 0) ? true : false;
          });

      std::sort(Components.begin(), Components.end(),
          [](const std::shared_ptr<VhdlEntity> &a, const std::shared_ptr<VhdlEntity> &b) -> bool {
            return (a->getName().compare(b->getName()) < 0) ? true : false;
          });

      openFile();

      /* 
       * Construct Entity Declaration
       */
      (*F) << "library ieee;\nuse ieee.std_logic_1164.all;\nuse ieee.numeric_std.all\n\n";

      (*F) << "entity " << Name << " is\n";
      (*F) << "\tport (\n";

      (*F) << "\t\t" << Clk->getName()   << " : in std_logic;\n";
      (*F) << "\t\t" << Reset->getName() <<  " : in std_logic";

      auto E = &(*InMem.rbegin());
      for (port_p P : InMem) {
        (*F) << ";\n" << P->getPortDecl("\t\t");
      }

      E = &(*InScalar.rbegin());
      for (port_p P : InScalar) {
        (*F) << ";\n" << P->getPortDecl("\t\t");
      }

      E = &(*OutMem.rbegin());
      for (port_p P : OutMem) {
        (*F) << ";\n" << P->getPortDecl("\t\t");
      }

      E = &(*OutScalar.rbegin());
      for (port_p P : OutScalar) {
        (*F) << ";\n" << P->getPortDecl("\t\t");
      }

      (*F) << "\n\t);\n";
      (*F) << "end entity " << Name << ";\n";

      /* 
       * Handle Architecture
       */

      // Components
      writeArch();

      closeFile();
    }
};

class VhdlKernel: public VhdlEntity {
  public:
    VhdlKernel( const std::string &Name, port_p Clk, port_p Reset ) : VhdlEntity(Name, Clk, Reset)  {

    }

    virtual void writeArch() const {

      (*F) << "architecture arch of " << Name << " is\n";
      for (auto &S : Signals) {
        (*F) << "\t" << S->getName() << " : std_logic_vector(" << S->W-1 << " downto 0;\n";
      }
      (*F) << "begin\n";

      for ( auto &C : Components ) {
        (*F) << "\t" << C->getName()+"Component" << ": entity " << C->getName() << " is \n";
        (*F) << "\tport map(\n";
        (*F) << "\t\t" << Clk->getName() << " => " << Clk->getName() << ",\n";
        (*F) << "\t\t" << Reset->getName() << " => " << Reset->getName();

        for ( port_p P : InScalar) {
          (*F) << ",\n\t\t" << P->getName() << " => " << P->getName();
        }
        (*F) << "\n\t);\n";
      }

      //Print all combinational statements
      for (auto &C : Combinational ) {
        (*F) << C->write("\t");
      }

      for (auto &C : ValueAssignments ) {
        (*F) << "\t" << C.first->getName() << " <= std_logic_vector(to_unsigned(" << C.second << ", " << C.first->getName() << "'length));\n";
      }

      for (auto &C : SignalAssignments ) {
        (*F) << "\t" << C.first->getName() << " <= " << C.second->getName() << ";\n";
      }


      (*F) << "end architecture arch;\n";
    }
};

class VhdlIntAdd : public VhdlCombinational {

  public:
    signal_p Target;
    signal_p Op0;
    signal_p Op1;

    VhdlIntAdd ( signal_p Target, signal_p Op0, signal_p Op1 ) : Target(Target), Op0(Op0), Op1(Op1)  {
    }

    virtual const std::string write(const std::string &Indent) {
      std::stringstream SS;

      SS << Indent << Target->getName() << " <= " << Op0->getName() << " + " << Op1->getName() << ";\n";

      return SS.str();
    }
};
#if 0
      if (Out.size() == 1) {
        auto &O = Out.front();
        (*F) << "\t" << O->getName() << " <= ";

        auto It = In.begin();
        if ( (*It)->isConst() ) {
          (*F) << (*It)->getName();
        } else {
          (*F) << (*It)->getName();
        }
        It++;
        while (It != In.end() ) {
          if ( (*It)->isConst() ) {
            (*F) << " + " << (*It)->getName();
          } else {
            (*F) << " + " << (*It)->getName();
          }
          It++;
        }
        (*F) << ";\n";
      } else {
        TODO("Multiple outputs");
      }
      if (Target && Src1 && Src2) {
        ss << "\t" << Target->getName() << " <= " << Src1->getName() << " + " << Src2->getName() << ";\n";
        return ss.str();
      } else
        return "invalid";
  }
#endif

class VhdlIntMul : public VhdlCombinational {

  public:
    signal_p Target;
    signal_p Op0;
    signal_p Op1;

    VhdlIntMul ( signal_p Target, signal_p Op0, signal_p Op1 ) : Target(Target), Op0(Op0), Op1(Op1)  {
    }

    virtual const std::string write(const std::string &Indent) {
      std::stringstream SS;

      SS << Indent << Target->getName() << " <= " << Op0->getName() << " * " << Op1->getName() << ";\n";

      return SS.str();
    }
};


} //ns oclacc

#endif /* VHDLGEN_H */
