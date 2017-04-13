#include <memory>
#include <sstream>
#include <cmath>
#include <unordered_set>
#include <map>
#include <set>

#include "../../HW/Kernel.h"

#include "Verilog.h"
#include "VerilogModule.h"
#include "Naming.h"
#include "VerilogMacros.h"
#include "DesignFiles.h"
#include "OperatorInstances.h"

#define DEBUG_TYPE "verilog"

using namespace oclacc;

VerilogModule::VerilogModule(Component &C) : Comp(C) {
}

VerilogModule::~VerilogModule() {
}

const std::string VerilogModule::declFooter() const {
  std::stringstream S;
  S << "endmodule " << " // " << Comp.getUniqueName() << "\n";
  return S.str();
}


BlockModule::BlockModule(Block &B) : VerilogModule(B), Comp(B), CriticalPath(0) {
  // Clean up Components
  ConstSignals << "// Constant signals\n";

  BlockSignals << "// Component signals\n";

  BlockComponents << "// Component instances\n";
}


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

// Block

/// \brief Separate functions because StreamPorts have to be instantiated
/// differently
const std::string BlockModule::declHeader() const {
  std::stringstream S;

  S << "module " << Comp.getName() << "(\n";

  Signal::SignalListTy Ports = getSignals(Comp);
  S << createPortList(Ports);

  S << "\n); // end ports\n";

  return S.str();
}

#if 0
const std::string BlockModule::declEnable() const {
  std::stringstream S;
  
  S << "// block enable\n";
  S << "wire enable_" << Comp.getName() << ";\n";
  S << "assign enable_" << Comp.getName() << " = ";

  const Block::CondListTy &C = Comp.getConds();
  const Block::CondListTy &NC = Comp.getNegConds();

  for (Block::CondConstItTy I=C.begin(), E=C.end(); I != E; ++I) {
    S << I->first->getUniqueName();
    if (std::next(I) != E) {
      S << " & ";
    }
  }
  for (Block::CondConstItTy I=NC.begin(), E=NC.end(); I != E; ++I) {
    S << "~" << I->first->getUniqueName();
    if (std::next(I) != E) {
      S << " & ";
    }
  }
  S << ";\n";
  S << "// end block enable\n";

  return S.str();
}
#endif

const std::string BlockModule::declConstValues() const {
  // Use a set to get unique definitions.
  std::unordered_set<std::string> SS;
  std::stringstream S;

  S << "// ConstVals\n";
  for (const const_p C : Comp.getConstVals()) {
    Signal CS(getOpName(*C), C->getBitWidth(), Signal::Local, Signal::Reg);

    std::string Decl = CS.getDefStr() + " = " + C->getBits();
    SS.insert(Decl);
  }

  for (const std::string &US : SS) {
    S << US << ";\n";
  }

  return S.str();
}

const std::string BlockModule::declFSMSignals() const {
  std::stringstream S;

  if (!CriticalPath) return "";

  S << "// FSM signals\n";
  S << "localparam state_free=0, state_busy=1, state_wait_output=2, state_wait_store=3, state_wait_load=4" << ";\n";

  // States
  Signal State("state", 3, Signal::Local, Signal::Reg);
  Signal NextState("next_state", 3, Signal::Local, Signal::Reg);
  S << State.getDefStr() << ";\n";
  S << NextState.getDefStr() << ";\n";


  // CriticalPath counter
  Signal Count("counter", std::ceil(std::log2(CriticalPath))-1, Signal::Local, Signal::Reg);
  S << Count.getDefStr() << ";\n";
  Signal CountEnabled("counter_enabled", 1, Signal::Local, Signal::Reg);
  S << CountEnabled.getDefStr() << ";\n";

  return S.str();
}

const std::string BlockModule::declFSM() const {
  std::stringstream S;

  // Needed when portlists are to be generated
  std::string Prefix;
  unsigned II = 1;

  // All _valid signals of Inputs and Outputs
  std::vector<std::string> ScalarInputNames;
  std::vector<std::string> ScalarOutputNames;

  for (scalarport_p P : Comp.getInScalars()) {
    if (P->isPipelined()) {
      ScalarInputNames.push_back(getOpName(P));
    }
  }

  for (scalarport_p P : Comp.getOutScalars()) {
    if (P->isPipelined())
      ScalarOutputNames.push_back(getOpName(P));
  }

  ////////////////////////////////////////////////////////////////////////////
  ////////////////////////////////////////////////////////////////////////////
  ////////////////////////////////////////////////////////////////////////////

  // Asynchronous state and output
  S << "always @(*)" << "\n";
  S << "begin\n";

  // Make sure outputs are logik
  S << Indent(II) << "next_state <= state;\n";
  S << Indent(II) << "counter_enabled <= 0;\n";

  S << Indent(II) << "case (state)" << "\n";
  S << Indent(II) << "state_free:" << "\n";
    S << Indent(++II) << "begin" << "\n";

    S << Indent(II++) << "if(\n";

    Prefix = "";
    for (const std::string &N : ScalarInputNames) {
        S << Prefix << Indent(II) << N << "_valid" << " == 1";
      Prefix = " && \n";
    }
    S << "\n" << Indent(--II) << ")" << "\n";

    // state_free
    S << Indent(++II) << "begin" << "\n";
    S << Indent(II) << "next_state <= state_busy" << ";\n";
    S << Indent(II--) << "end" << "\n";


    S << Indent(II--) << "end" << "\n";

  S << Indent(II) << "state_busy:" << "\n";
    S << Indent(++II) << "begin" << "\n";
    S << Indent(II) << "counter_enabled <= 1;\n";

    // Enable outputs starting at correct cycle
    for (streamindex_p SI : Comp.getStaticOutStreamIndices()) {
      assert(SI->getIns().size() == 1);

      base_p Val = SI->getIn(0);
      const std::string Name = getOpName(Val);
      unsigned C = getReadyCycle(Name);

      S << Indent(II) << "// " << Name << "\n";
      S << Indent(II++) << "if (counter >= " << C << " && " << Name << "_fin == 0)\n";
      S << Indent(II--) << "next_state <= state_wait_store;\n";
    }

    // state_wait_output
    S << Indent(II) << "if (counter == " << CriticalPath <<") next_state <= state_wait_output;\n";

    // state_

    S << Indent(II--) << "end" << "\n";

  S << Indent(II) << "state_wait_output:" << "\n";
    if (Comp.getOutScalars().size()) {
      S << Indent(++II) << "begin" << "\n";
      
      // When all outputs and stores are acknoledged, return to state_free
      Prefix = "";
      S << Indent(II++) << "if (\n";
      for (const std::string N : ScalarOutputNames) {
        S << Prefix << Indent(II) << N << "_fin == 1";
        Prefix = " &&\n";
      }
      S << "\n" << Indent(II) << ")\n";
      S << Indent((II--)+1) << "next_state <= state_free;\n";

      S << Indent(II--) << "end" << "\n";
    } else {
      S << Indent(II+1) << "next_state <= state_free;\n";
    }


  S << Indent(II) << "state_wait_store:" << "\n";
    BEGIN(S);

      for (dynamicstreamindex_p SI : Comp.getDynamicOutStreamIndices()) {
        streamport_p SP = SI->getStream();
        base_p Val = SI->getIn(0);

        const std::string Name = getOpName(SI);
        const std::string StreamName = getOpName(SP);
        const std::string ValName = getOpName(Val);

        unsigned C = getReadyCycle(Name);

        S << Indent(II) << "if (counter >= " << C << " && " << Name << "_fin == 0)\n";
          BEGIN(S);
          S << Indent(II) << Name << "_buf <= " << ValName << ";\n";
          END(S);
      }
    END(S);

  S << Indent(II) << "state_wait_load:" << "\n";
    BEGIN(S);
    END(S);


  S << Indent(II) << "endcase" << "\n";
  END(S);

  ////////////////////////////////////////////////////////////////////////////
  ////////////////////////////////////////////////////////////////////////////
  ////////////////////////////////////////////////////////////////////////////


  // Synchronous next_state
  S << "always @(posedge clk)" << "\n";
  S << "begin\n";
  // Reset state
  S << Indent(II) << "if (rst)\n";
    S << Indent(++II) << "begin\n";
    S << Indent(II) << "state <= state_free;" << "\n";

    // Input buffers and valids
    for (const std::string &N : ScalarInputNames) {
      S << Indent(II) << N << " <= '0;\n";
      S << Indent(II) << N << "_valid <= 0" << ";\n";
    }

    // Outout buffers and valids
    for (const std::string &N : ScalarOutputNames) {
      S << Indent(II) << N << " <= '0;\n";
      S << Indent(II) << N << "_valid <= 0;\n";
      S << Indent(II) << N << "_fin <= 0;\n";
    }

    // counter
    S << Indent(II) << "counter <= '0;\n";

    S << Indent(II--) << "end\n";
  S << Indent(II) << "else\n";
    S << Indent(++II) << "begin\n";
    S << Indent(II) << "state <= next_state;" << "\n";

    // Synchronous outputs

    // Assign ack only for a single cycle
    for (const std::string &N : ScalarInputNames) {
      S << Indent(II) << N << "_ack <= 0;\n";
    }

    // Reset status signals in state_free
    S << Indent(II) << "if (state == state_free)\n";
    BEGIN(S);
    for (const std::string &N : ScalarInputNames) {
      S << Indent(II) << N << " <= '0;\n";
      S << Indent(II) << N << "_valid <= 0;\n";
    }
    for (const std::string &N : ScalarOutputNames) {
      S << Indent(II) << N << " <= '0;\n";
      S << Indent(II) << N << "_valid <= 0;\n";
      S << Indent(II) << N << "_fin <= 0;\n";
    }
    END(S);

    // Allow others to acknowledge an output port in every state except state_free
    S << Indent(II) << "if (state != state_free)\n";
    BEGIN(S);
    for (const std::string &N : ScalarOutputNames) {
      S << Indent(II) << "if (" << N << "_ack == 1)\n";
          BEGIN(S);
          S << Indent(II) << N << " <= '0;\n";
          S << Indent(II) << N << "_fin <= 1;\n";
          S << Indent(II) << N << "_valid <= 0;\n";
          END(S);
    }
    END(S);

    S << Indent(II) << "case (state)" << "\n";
    S << Indent(II) << "state_free:" << "\n";
      BEGIN(S);

      // Buffer Inputs

      for (const std::string &N : ScalarInputNames) {
        S << Indent(II) << "if (" << N << "_unbuf_valid" << ")\n";
        BEGIN(S);
        S << Indent(II) << N << "_ack" << " <= 1;\n";
        S << Indent(II) << N << " <= " << N << "_unbuf;\n";
        S << Indent(II) << N << "_valid" << " <= 1;\n";
        END(S);
      }
      S << Indent(II--) << "end" << "\n";

    S << Indent(II) << "state_busy:" << "\n";
      BEGIN(S);
      S << Indent(II) << "if (counter_enabled)\n";
        BEGIN(S);
        S << Indent(II+1) << "if (counter < " << CriticalPath << ") counter <= counter + 1;\n";
        END(S);

        // Output gets ready, set valid and buffer output values, then wait for
        // ack
        for (const scalarport_p P : Comp.getOutScalars()) {
          assert(P->getIns().size() == 1);

          const std::string Name = getOpName(P);
          unsigned C = getReadyCycle(Name);

          base_p Val = P->getIn(0);

          const std::string VName = getOpName(Val);


          S << Indent(II) << "if (counter >= " << C << " && " << Name << "_valid == 0 && " << Name << "_fin == 0)\n";
            BEGIN(S);
            S << Indent(II) << Name << " <= " << VName << ";\n";
            S << Indent(II) << Name << "_valid <= 1;\n";
            END(S);
        }

        for (streamindex_p SI : Comp.getStaticOutStreamIndices()) {
          assert(SI->getIns().size() == 1);

          base_p Val = SI->getIn(0);
          const std::string Name = getOpName(Val);
          unsigned C = getReadyCycle(Name);
        }

      END(S);
    
    S << Indent(II) << "state_wait_output:" << "\n";
      S << Indent(++II) << "begin" << "\n";
      S << Indent(II--) << "end\n";

    S << Indent(II) << "state_wait_store:" << "\n";
      S << Indent(++II) << "begin" << "\n";
      S << Indent(II--) << "end\n";

    S << Indent(II) << "state_wait_load:" << "\n";
      S << Indent(++II) << "begin" << "\n";
      S << Indent(II--) << "end\n";


    S << Indent(II) << "endcase" << "\n";
    END(S);
  END(S);

  return S.str();
}

const std::string BlockModule::declPortControlSignals() const {
  std::stringstream S;

  S << "// InScalar buffer\n";
  for (const scalarport_p P : Comp.getInScalars()) {
    if (!P->isPipelined()) continue;

    Signal SP(getOpName(P), P->getBitWidth(), Signal::Local, Signal::Reg);
    S << SP.getDefStr() << ";\n";

    Signal SV(getOpName(P)+"_valid", 1, Signal::Local, Signal::Reg);
    S << SV.getDefStr() << ";\n";
  }

  S << "// InStream buffer\n";
  for (const streamindex_p P : Comp.getInStreamIndices()) {
    Signal SP(getOpName(P), P->getBitWidth(), Signal::Local, Signal::Reg);
    S << SP.getDefStr() << ";\n";

    Signal SV(getOpName(P)+"_valid", 1, Signal::Local, Signal::Reg);
    S << SV.getDefStr() << ";\n";
  }

  S << "// OutScalar internal\n";
  for (const scalarport_p P : Comp.getOutScalars()) {
    Signal SF(getOpName(P)+"_fin", 1, Signal::Local, Signal::Reg);
    S << SF.getDefStr() << ";\n";
  }

  S << "// OutStream internal\n";
  for (const streamindex_p P : Comp.getOutStreamIndices()) {
    Signal SF(getOpName(P)+"_fin", 1, Signal::Local, Signal::Reg);
    S << SF.getDefStr() << ";\n";
  }

  return S.str();
}

void BlockModule::schedule(const OperatorInstances &I) {
  const HW::HWListTy Ops = Comp.getOpsTopologicallySorted();

  NDEBUG("Traversation sequence:");
  for (base_p P : Ops) {
    NDEBUG(P->getUniqueName());
  }

      
}

#if 0
void BlockModule::schedule(const OperatorInstances &I) {
  // Fill all HW objects in worklist.
  HW::HWListTy C;

  for (const_p C : Comp.getConstVals()) {
    ReadyMap[C->getUniqueName()] = 0;
  }

  for (port_p P : Comp.getInScalars()) {
    const HW::HWListTy Outs = P->getOuts();
    C.insert(C.end(), Outs.begin(), Outs.end());

    ReadyMap[P->getUniqueName()] = 0;
  }

  while (C.size() != 0) {
    const base_p Curr = C.front();
    C.erase(C.begin());

    const std::string HWName = Curr->getUniqueName();

    unsigned Max = 0;
    bool allPredsAvailable=true;

    for (const base_p P : Curr->getIns()) {
      int InReady = 0;
      const std::string InName = P->getUniqueName();

      InReady = getReadyCycle(InName);
      //NDEBUG(InName << " " << InReady);

      if (InReady == -1) {
        allPredsAvailable = false;
        break;
      }

      unsigned InCycles = 0;

      const std::string OpName = getOpName(P);

      op_p Op = I.getOperatorForHW(OpName);

      if (Op) {
        InCycles = Op->Cycles;
        //NDEBUG(InName << " takes " << InCycles);
      } else 
        InCycles = 0;

      Max = std::max(InReady+InCycles, Max);
    }


    if (allPredsAvailable) {
      ReadyMap[HWName] = Max;

      op_p Op = I.getOperatorForHW(HWName);

      if (Op) {
        NDEBUG(Op->Name);
        Max += Op->Cycles;
      }

      CriticalPath = std::max(Max, CriticalPath);

      //NDEBUG(HWName << " @ " << Max);
    } else
      C.push_back(Curr);

    const HW::HWListTy Outs = Curr->getOuts();
    C.insert(C.end(), Outs.begin(), Outs.end());

  }

  for (const ReadyMapElemTy &E : ReadyMap) {
    NDEBUG(E.first << " @ " << E.second);
  }

  NDEBUG("critical path: " <<  CriticalPath);
}
#endif

int BlockModule::getReadyCycle(const std::string OpName) const {
  ReadyMapConstItTy E = ReadyMap.find(OpName);
  if (E == ReadyMap.end()) return -1;

  return E->second;
}

#ifdef DEBUG_TYPE
#undef DEBUG_TYPE
#endif
