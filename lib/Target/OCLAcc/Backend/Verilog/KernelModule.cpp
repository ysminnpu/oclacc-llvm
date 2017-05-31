#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/CommandLine.h"

#include <memory>
#include <sstream>
#include <cmath>
#include <unordered_set>
#include <map>
#include <set>

#include "../../HW/Kernel.h"

#include "Flopoco.h"
#include "Verilog.h"
#include "VerilogModule.h"
#include "Naming.h"
#include "VerilogMacros.h"
#include "DesignFiles.h"
#include "OperatorInstances.h"

#include "VerilogModule.h"

#define DEBUG_TYPE "verilog"

using namespace oclacc;

KernelModule::KernelModule(Kernel &K) : VerilogModule(K), Comp(K) {
}

// Kernel

const std::string KernelModule::declHeader() const {
  std::stringstream S;

  S << "module " << Comp.getName() << "(\n";

  Signal::SignalListTy Ports = getSignals(Comp);
  
  // All Kernel Ports are wires.
  for (Signal& S : Ports) {
    S.Type = Signal::Wire;
  }
  S << createPortList(Ports);

  S << "\n); // end ports\n";

  return S.str();
}

/// \brief Declare Wires for the Blocks' Ports and connect them with the
/// Kernel inputs
///
/// Iterate over all Blocks' ports and create wires for each port.
/// Finally connect the kernel outsputs.
///
/// All non-pipelined ScalarPorts and StreamPorts already exist as kernel ports.
///
/// If a ScalarPort has multiple inputs, generate a Muxer to select a single
/// one, depending on the condition which must be true for a transition of the
/// originating Block to the input's Block.
///
const std::string KernelModule::declBlockWires() const {
  std::stringstream S;

  std::stringstream Wires;
  Wires << "// Block wires\n";

  std::stringstream Assignments;
  Assignments << "// Block assignments\n";

  std::stringstream Logic;
  Assignments << "// Block port muxer\n";

  // Collect all ScalarPorts with multiple Inputs or multiple Outputs.
  // Inputs: generate Multiplexer, use <uniquename>_<src>_<dest> as Signal name
  // Outputs: Acks must be logically OR'ed.
  std::unordered_set<scalarport_p> SingleIns;
  std::unordered_set<scalarport_p> SingleOuts;

  std::unordered_set<scalarport_p> MultipleIns;
  std::unordered_set<scalarport_p> MultipleOuts;

  typedef std::map<std::string, std::vector<std::string> > OrMapTy;
  typedef std::pair<std::string, std::vector<std::string> > OrMapElemTy;
  OrMapTy OrMap;

  typedef std::pair<Signal, Signal> SigPairTy;

  typedef std::pair<std::string, std::vector<SigPairTy> > MuxCondElemTy;
  typedef std::map<std::string, std::vector<SigPairTy > > MuxCondTy;
  
  typedef std::pair<std::string, MuxCondTy > MuxElemTy;
  typedef std::map<std::string, MuxCondTy > MuxTy;

  MuxTy Muxer;

  for (block_p B : Comp.getBlocks()) {
    for (scalarport_p P : B->getInScalars()) {
      if (!P->isPipelined())
        continue;

      unsigned NumIns = P->getIns().size();
      assert(NumIns && "Scalar Input in Block without source");

      if (NumIns == 1)
        SingleIns.insert(P);
      else
        MultipleIns.insert(P);
    }
    for (scalarport_p P : B->getOutScalars()) {
      if (!P->isPipelined())
        continue;

      unsigned NumOuts = P->getOuts().size();
      assert(NumOuts && "Scalar Output in Block without sink");

      if (NumOuts == 1)
        SingleOuts.insert(P);
      else
        MultipleOuts.insert(P);
    }
  }

  // Input with a single source but maybe its source has multiple sinks
  for (scalarport_p Sink : SingleIns) {
    scalarport_p Src = std::static_pointer_cast<ScalarPort>(Sink->getIn(0));

    unsigned NumSourceOuts = Src->getOuts().size();

    Signal::SignalListTy SinkPortSigs = getInSignals(Sink);
    Signal::SignalListTy SrcPortSigs;

    // Kernel inputs use the same naming scheme as block inputs
    if (Src->getParent()->isKernel())
      SrcPortSigs = getInSignals(Src);
    else
      SrcPortSigs = getOutSignals(Src);

    assert(SrcPortSigs.size() == SinkPortSigs.size());

    for (Signal::SignalListConstItTy
        SinkI = SinkPortSigs.begin(),
        SrcI = SrcPortSigs.begin(),
        SrcE = SrcPortSigs.end();
        SrcI != SrcE;
        ++SinkI, ++SrcI) {

      // Define Block's Port. All Ports are wires.
      Signal LocDef(SinkI->Name, SinkI->BitWidth, Signal::Local, Signal::Wire);
      Wires << LocDef.getDefStr() << ";\n";

      if (SinkI->Direction == Signal::Out) {
        if (NumSourceOuts == 1)
          Assignments << "assign " << SrcI->Name << " = " << SinkI->Name << ";\n"; 
        //else
        //  OrMap[SrcI->Name].push_back(TI->Name);
      } else {
          Assignments << "assign " << SinkI->Name << " = " << SrcI->Name << ";\n"; 
      }
    }   
  }
  
  // Output with a single sink but its sink may have multiple sources
  for (scalarport_p P : SingleOuts) {
    scalarport_p SP = std::static_pointer_cast<ScalarPort>(P->getOut(0));

    unsigned NumSinkInputs = SP->getIns().size();

    Signal::SignalListTy ThisPortSigs = getOutSignals(P);
    
    // Multiple Sink imputs are handled later
    Signal::SignalListTy SinkPortSigs = getInSignals(SP);

    assert(SinkPortSigs.size() == ThisPortSigs.size());

    for (Signal::SignalListConstItTy
        TI = ThisPortSigs.begin(),
        SI = SinkPortSigs.begin(),
        SE = SinkPortSigs.end();
        SI != SE;
        ++TI, ++SI) {

      Signal LocDef(TI->Name, TI->BitWidth, Signal::Local, Signal::Wire);
      Wires << LocDef.getDefStr() << ";\n";

      if (NumSinkInputs == 1) {
        if (TI->Direction == Signal::Out)
          Assignments << "assign " << SI->Name << " = " << TI->Name << ";\n"; 
        else
          Assignments << "assign " << TI->Name << " = " << SI->Name << ";\n"; 
      } 
    } 
  }

  for (scalarport_p Sink : MultipleIns) {
    // Declare Signals for Mux Output and actual InScalar
    Signal::SignalListTy SinkPortSigs = getInSignals(Sink);
    for (const Signal &S : SinkPortSigs) {
        if (S.Direction == Signal::Out) {
          Signal LocDef(S.Name, S.BitWidth, Signal::Local, Signal::Wire);
          Wires << LocDef.getDefStr() << ";\n";
        } else {
          Signal LocDef(S.Name, S.BitWidth, Signal::Local, Signal::Reg);
          Wires << LocDef.getDefStr() << ";\n";
        }
    }

    for (base_p SrcTmp : Sink->getIns()) {
      scalarport_p Src = std::static_pointer_cast<ScalarPort>(SrcTmp);

      Signal::SignalListTy SinkMuxPortSigs = getInMuxSignals(Sink, Src);
      Signal::SignalListTy SrcPortSigs = getOutSignals(Src);

      assert (SrcPortSigs.size() == SinkPortSigs.size());
      assert (SrcPortSigs.size() == SinkMuxPortSigs.size());

      for (Signal::SignalListConstItTy
          SrcI = SrcPortSigs.begin(),
          SinkI = SinkPortSigs.begin(),
          SinkMuxI = SinkMuxPortSigs.begin(),
          SinkE = SinkPortSigs.end();
          //
          SinkI != SinkE;
          //
          ++SinkI, ++SinkMuxI, ++SrcI) {

        if (SrcI->Direction == Signal::Out) {
          Signal LocDef(SinkMuxI->Name, SinkMuxI->BitWidth, Signal::Local, Signal::Wire);
          Wires << LocDef.getDefStr() << ";\n";
        } else {
          Signal LocDef(SinkMuxI->Name, SinkMuxI->BitWidth, Signal::Local, Signal::Reg);
          Wires << LocDef.getDefStr() << ";\n";
        }

        if (SrcI->Direction == Signal::Out)
          Assignments << "assign " << SinkMuxI->Name << " = " << SrcI->Name << ";\n"; 
        else {
          if (Src->getOuts().size() > 1)
            OrMap[SrcI->Name].push_back(SinkMuxI->Name);
        }

        // Sink has multiple inputs, must be added to multiplexer
        block_p SrcB = std::static_pointer_cast<Block>(Src->getParent());
        block_p SinkB = std::static_pointer_cast<Block>(Sink->getParent());

        scalarport_p Cond = SinkB->getCondReachedByBlock(SrcB);
        std::string Neg = "";
        if (!Cond) {
          Cond = SinkB->getNegCondReachedByBlock(SrcB);
          Neg = "!";
        }
        assert(Cond);

        Signal::SignalListTy CondSigs = getInSignals(Cond);

        std::string CondName = Neg+CondSigs[0].Name;

        Muxer[getOpName(Sink)][CondName].push_back(std::make_pair(*SinkI, *SinkMuxI));
      } 
    }
  }

  for (scalarport_p Src : MultipleOuts) {
    Signal::SignalListTy SrcPortSigs = getOutSignals(Src);

    for (const Signal &S : SrcPortSigs) {
      Signal LocDef(S.Name, S.BitWidth, Signal::Local, Signal::Wire);
      Wires << LocDef.getDefStr() << ";\n";
    }


    for (base_p SinkTmp : Src->getOuts()) {
      scalarport_p Sink = std::static_pointer_cast<ScalarPort>(SinkTmp);

      Signal::SignalListTy SinkPortSigs = getInSignals(Sink);

      assert(SinkPortSigs.size() == SrcPortSigs.size());

      if (Sink->getIns().size() == 1) {
        for (Signal::SignalListConstItTy
            SrcI = SrcPortSigs.begin(),
            SinkI = SinkPortSigs.begin(),
            SrcE = SrcPortSigs.end();
            SrcI != SrcE;
            ++SrcI, ++SinkI) {

          if (SrcI->Direction == Signal::Out) {
            //Assignments << "assign " << SinkI->Name << " = " << SrcI->Name << ";\n"; 
          } else
            OrMap[SrcI->Name].push_back(SinkI->Name);
            //Assignments << "assign " << SrcI->Name << " = " << SinkI->Name << ";\n"; 
        }
      } else {
        //multiplexer already created. Use Mux Portnames.
      }
    } 
  }

  for (const OrMapElemTy &O : OrMap) {
    Assignments << "assign " << O.first << " = ";
    std::string Prefix = "";
    for(const std::string &S : O.second) {
      Assignments << Prefix << S;
      Prefix = " | ";
    }
    Assignments << ";\n";
  }

  unsigned II = 1;
  for (const MuxElemTy &O : Muxer) {
    Logic << "// Muxer " << O.first << "\n";
    Logic << "always@(*)\n";
      BEGIN(Logic);
      // default outputs to zero
      for (const MuxCondElemTy &ME : O.second) {
        for (const SigPairTy &S : ME.second) {
          if (S.first.Direction == Signal::In)
            Logic << Indent(II) << S.first.Name << " <= '0;\n";
        }
        break;
      }
      for (const MuxCondElemTy &ME : O.second) {
        for (const SigPairTy &S : ME.second) {
          if (S.first.Direction == Signal::Out)
            Logic << Indent(II) << S.second.Name << " <= '0;\n";
        }
      }

      std::string Prefix = "";
      for (const MuxCondElemTy &ME : O.second) {
        Logic << Indent(II) << Prefix << "if (";
        Logic << ME.first << " && "; 

        std::string CondName = ME.first;
        if (CondName.at(0) == '!')
          CondName.erase(0,1);
        Logic << CondName << "_valid)\n";

        BEGIN(Logic);
        for (const SigPairTy &S : ME.second) {
          // first: Sink, second: Source
          if (S.first.Direction == Signal::Out)
            Logic << Indent(II) << S.second.Name << " <= " << S.first.Name << ";\n"; 
          else
            Logic << Indent(II) << S.first.Name << " <= " << S.second.Name << ";\n"; 
        }
        END(Logic);

        Prefix = "else ";
      }
      
      END(Logic);
  }

  // Declare the wires for all barriers in the kernel
  Signal::SignalListTy BS;
  for (block_p BB : Comp.getBlocks()) {
    barrier_p B = BB->getBarrier();
    if (B != nullptr) {
      Signal::SignalListTy SL = getSignals(B);
      BS.insert(BS.end(), SL.begin(), SL.end());
    }
  }
  if (!BS.empty()) {
    Wires << "// Barrier signals\n";
    for (Signal &S : BS) {
      S.Direction = Signal::Local;
      Wires << S.getDefStr() << ";\n";
    }
  }

  S << Wires.str();
  S << Assignments.str();
  S << Logic.str();
  return S.str();
}

/// \brief Instantiate blocks with ports
///
/// Use the Port's name as it is unique. We already created wires for
/// each Port and we will connect them afterwards.
///
/// - ScalarInputs with multiple sources are connected to a new multiplexer.
/// - Non-pipelined ScalarInputs need no synchronization signals.
/// - Streams have separate address ports for each store and load

const std::string KernelModule::instBlocks() const {
  std::stringstream SBlock;

  SBlock << "// Block instantiations\n";

  for (block_p B : Comp.getBlocks()) {
    const std::string BName = B->getUniqueName();

    // Walk through all Ports of the Block.
    Signal::SignalListTy Ports = getSignals(B);

    // Blocks must have any input and output
    assert(Ports.size());

    // Block instance name
    SBlock << B->getName() << " " << B->getUniqueName() << "(\n";

    std::string Linebreak = "";
    for (const Signal &P : Ports) {
      const std::string PName = P.Name;

      SBlock << Linebreak << I(1) << "." << PName << "(" << PName << ")";
      Linebreak = ",\n";
    }

    SBlock << "\n" << ");\n";
  }
  return SBlock.str();
}
