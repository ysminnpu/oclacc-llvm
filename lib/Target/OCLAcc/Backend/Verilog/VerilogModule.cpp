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

#define DEBUG_TYPE "verilog"

static cl::opt<bool> NoPreserveLoadOrder("no-preserve-load-order", cl::init(false), cl::desc("Keep the order of memory loads.") );

using namespace oclacc;

VerilogModule::VerilogModule(Component &C) : Comp(C) {
}

VerilogModule::~VerilogModule() {
}

void VerilogModule::genTestBench() const {
}

const std::string VerilogModule::declFooter() const {
  std::stringstream S;
  S << "endmodule " << " // " << Comp.getUniqueName() << "\n";
  return S.str();
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

BlockModule::BlockModule(Block &B) : VerilogModule(B), Comp(B), CriticalPath(0) {
  // Clean up Components
  ConstSignals << "// Constant signals\n";

  BlockSignals << "// Component signals\n";

  BlockAssignments << "// Block assignments\n";

  LocalOperators << "// Local operators\n";

  BlockComponents << "// Component instances\n";
}


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


  S << "// FSM signals\n";
  S << "localparam state_free=0, state_busy=1, state_wait_load=2, state_wait_store=3, state_wait_output=4" << ";\n";

  // States
  Signal State("state", 3, Signal::Local, Signal::Reg);
  Signal NextState("next_state", 3, Signal::Local, Signal::Reg);
  S << State.getDefStr() << ";\n";
  S << NextState.getDefStr() << ";\n";

  // If CriticalPath is 0, have at least one bit.
  unsigned CounterWidth = 1;
  if (CriticalPath > 1)
    CounterWidth = static_cast<unsigned int>(std::ceil(std::log2(CriticalPath)));

  // CriticalPath counter
  Signal Count("counter", CounterWidth, Signal::Local, Signal::Reg);
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
      BEGIN(S);
      S << Indent(II) << "next_state <= state_busy" << ";\n";
      END(S);


    END(S);

  S << Indent(II) << "state_busy:" << "\n";
    BEGIN(S);
    S << Indent(II) << "counter_enabled <= 1;\n";

    // Enable outputs starting at correct cycle
    for (storeaccess_p SI : Comp.getStores()) {
      const std::string Name = getOpName(SI);

      unsigned C = getReadyCycle(Name);

      S << Indent(II) << "// " << Name << "\n";
      S << Indent(II) << "if (counter == " << C << " && " << Name << "_fin == 0)\n";
      S << Indent(II+1) << "next_state <= state_wait_store;\n";
    }

    for (loadaccess_p LI : Comp.getLoads()) {
      const std::string Name = getOpName(LI);

      S << Indent(II) << "if (counter == " << getReadyCycle(Name) <<" && " << Name << "_valid == 0) next_state <= state_wait_load;\n";
    }

    // state_wait_output
    S << Indent(II) << "if (counter == " << CriticalPath <<") next_state <= state_wait_output;\n";

    // state_

    END(S);

  S << Indent(II) << "state_wait_output:" << "\n";
    if (Comp.getOutScalars().size()) {
      BEGIN(S);
      
      // When all outputs and stores are acknoledged, return to state_free
      Prefix = "";
      S << Indent(II++) << "if (";
      for (const std::string N : ScalarOutputNames) {
        S << Prefix << N << "_fin == 1";
        Prefix = "\n" + Indent(II) + "&& ";
      }
      S << ")\n";
      S << Indent((II--)+1) << "next_state <= state_free;\n";

      END(S)
    } else {
      S << Indent(II+1) << "next_state <= state_free;\n";
    }


  S << Indent(II) << "state_wait_store:" << "\n";
    BEGIN(S);
    if (Comp.hasStores()) {
      BEGIN(S);
      S << Indent(II) << "if ((";
      Prefix = "";
      for (storeaccess_p SA : Comp.getStores()) {
        const std::string SName = getOpName(SA);
        S << Prefix << SName << "_fin";
        Prefix = " & ";
      }
      S << ") == 0)\n";
      S << Indent(II+1) << "next_state <= state_busy;\n";
      END(S);
    }
    END(S);
  S << Indent(II) << "state_wait_load:" << "\n";
    BEGIN(S);
    if (Comp.hasLoads()) {
      BEGIN(S);
      S << Indent(II) << "if ((";
      Prefix = "";
      for (loadaccess_p LA : Comp.getLoads()) {
        const std::string LName = getOpName(LA);
        S << Prefix << LName << "_address_valid && " << LName << "_unbuf_valid";
        Prefix = "\n" + Indent(II) + " & ";
      }
      S << ") == 0)\n";
      S << Indent(II+1) << "next_state <= state_busy;\n";

      END(S);
    }
    END(S);


  S << Indent(II) << "endcase" << "\n";
  END(S);

  ////////////////////////////////////////////////////////////////////////////
  ////////////////////////////////////////////////////////////////////////////
  ////////////////////////////////////////////////////////////////////////////


  // Synchronous next_state
  S << "//FSM next_state" << "\n";
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
      S << Indent(II) << N << "_ack <= 0" << ";\n";
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

    // Reset state after completion
    S << Indent(II) << "if (next_state == state_free)\n";
      BEGIN(S);
      for (const std::string &N : ScalarInputNames) {
        S << Indent(II) << N << " <= '0;\n";
        S << Indent(II) << N << "_valid <= 0;\n";
      }
      // Reset status signals in state_free
      for (const std::string &N : ScalarOutputNames) {
        S << Indent(II) << N << " <= '0;\n";
        S << Indent(II) << N << "_valid <= 0;\n";
        S << Indent(II) << N << "_fin <= 0;\n";
      }
      END(S);

    S << Indent(II) << "case (state)" << "\n";
    S << Indent(II) << "state_free:" << "\n";
      BEGIN(S);

      // Buffer Inputs

      // Only set ack for a single cycle and do not wait for state to change
      for (const std::string &N : ScalarInputNames) {
        S << Indent(II) << "if (" << N << "_unbuf_valid == 1 && " << N << "_valid == 0)\n";
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

    S << Indent(II) << "// Reset counter\n";
    S << Indent(II) << "if (next_state == state_free) counter <= '0;\n";
    END(S);
  END(S);

  return S.str();
}

const std::string BlockModule::declStores() const {
  std::stringstream S;
  S << "// Store processes\n";

  for (storeaccess_p SA : Comp.getStores()) {
    assert(SA->getIns().size() == 1 && "Stores may only have a single input");

    const std::string Name = getOpName(SA);
    const streamindex_p Index = SA->getIndex();

    std::string IndexName;

    if (staticstreamindex_p SI = std::dynamic_pointer_cast<StaticStreamIndex>(Index)) {
      IndexName = std::to_string(SI->getIndex());
    }
    else {
      dynamicstreamindex_p DI = std::static_pointer_cast<DynamicStreamIndex>(Index);
      IndexName = getOpName(DI->getIndex());

    }

    const std::string ValueName = getOpName(SA->getValue());

    unsigned Clk = getReadyCycle(Name);

    unsigned II = 0;
    S << "// StoreAccess " << Name << "\n";
    S << "always @(posedge clk)\n";
      BEGIN(S);
      S << Indent(II) << "if (rst==1)\n";
        BEGIN(S);
        S << Indent(II) << Name << "_address = '0;\n";
        S << Indent(II) << Name << "_buf = '0;\n";
        S << Indent(II) << Name << "_valid = 0;\n";
        S << Indent(II) << Name << "_running = 0;\n";
        S << Indent(II) << Name << "_fin = 0;\n";
        END(S);

      S << Indent(II) << "else\n";
        BEGIN(S);
        S << Indent(II) << "if (counter == " << Clk << ")\n";
          BEGIN(S);
          S << Indent(II) << Name << "_address = " << IndexName << ";\n";
          S << Indent(II) << Name << "_buf = " << ValueName << ";\n";
          S << Indent(II) << Name << "_valid = 1;\n";
          S << Indent(II) << Name << "_running = 1;\n";
          END(S);

        S << Indent(II) << "if (" << Name << "_running == 1 && " << Name << "_ack == 1)\n";
          BEGIN(S);
          S << Indent(II) << Name << "_address = '0;\n";
          S << Indent(II) << Name << "_buf = '0;\n";
          S << Indent(II) << Name << "_valid = 0;\n";
          S << Indent(II) << Name << "_running = 0;\n";
          S << Indent(II) << Name << "_fin = 1;\n";
          END(S);
        END(S);
    END(S);
  }

  return S.str();
}

const std::string BlockModule::declLoads() const {
  std::stringstream S;
  S << "// Load processes\n";

  for (loadaccess_p LA : Comp.getLoads()) {
    const std::string Name = getOpName(LA);
    const streamindex_p Index = LA->getIndex();

    std::string IndexName;

    if (staticstreamindex_p SI = std::dynamic_pointer_cast<StaticStreamIndex>(Index)) {
      IndexName = std::to_string(SI->getIndex());
    }
    else {
      dynamicstreamindex_p DI = std::static_pointer_cast<DynamicStreamIndex>(Index);
      IndexName = getOpName(DI->getIndex());
    }

    unsigned Clk = getReadyCycle(Name);

    unsigned II = 0;
    S << "// LoadAccess " << Name << "\n";
    S << "always @(posedge clk)\n";
      BEGIN(S);
      S << Indent(II) << "if (rst==1)\n";
        BEGIN(S);
        // local buffer
        S << Indent(II) << Name << " = '0;\n";
        S << Indent(II) << Name << "_valid = 0;\n";
        // ports
        S << Indent(II) << Name << "_address = '0;\n";
        S << Indent(II) << Name << "_address_valid = 0;\n";
        S << Indent(II) << Name << "_ack = 0;\n";
        END(S);

      S << Indent(II) << "else\n";
        BEGIN(S);
        // Set signal for a single cycle
        S << Indent(II) << Name << "_ack = 0;\n";

        S << Indent(II) << "if (counter == " << Clk << " && " << Name << "_address_valid == 0)\n";
          BEGIN(S);
          S << Indent(II) << Name << "_address = " << IndexName << ";\n";
          S << Indent(II) << Name << "_address_valid = 1;\n";
          END(S);

        S << Indent(II) << "if (" << Name << "_address_valid == 1 && " << Name << "_unbuf_valid == 1)\n";
          BEGIN(S);
          S << Indent(II) << Name << " = " << Name << "_unbuf;\n";
          S << Indent(II) << Name << "_ack = 1;\n";
          S << Indent(II) << Name << "_valid = 1;\n";
          S << Indent(II) << Name << "_address = '0;\n";
          S << Indent(II) << Name << "_address_valid = 0;\n";
          END(S);
      END(S);

    END(S);
  }
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

  S << "// Load buffer\n";
  for (const loadaccess_p P : Comp.getLoads()) {
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

  S << "// Store internal\n";
  for (const storeaccess_p P : Comp.getStores()) {
    Signal SF(getOpName(P)+"_fin", 1, Signal::Local, Signal::Reg);
    S << SF.getDefStr() << ";\n";
    Signal SR(getOpName(P)+"_running", 1, Signal::Local, Signal::Reg);
    S << SR.getDefStr() << ";\n";
  }

  return S.str();
}

void BlockModule::schedule(const OperatorInstances &I) {
  typedef Identifiable::UIDTy UIDTy;
  typedef std::set<std::pair<UIDTy, UIDTy> > AddedConsTy;
  AddedConsTy AddedCons;

  // Add additional dependencies between Loads to keep their order and remove
  // them afterwards.
  if (!NoPreserveLoadOrder) {
    StreamPort::LoadListTy LL = Comp.getLoads();

    for (StreamPort::LoadListTy::iterator LI = LL.begin(), LE = LL.end(); LI != LE; ++LI) {
      if (std::next(LI) == LE) break;

      loadaccess_p This = *LI;
      loadaccess_p Next = *(std::next(LI));

      if (This->getStream() == Next->getStream()) {
        AddedCons.insert(std::make_pair(This->getUID(), Next->getUID()));
        This->addOut(Next);
        Next->addIn(This);
      }
    }
  }

  HW::HWListTy Ops = Comp.getOpsTopologicallySorted();

  for (base_p P : Ops) {
    int MaxPreds = 0;

    for (base_p In : P->getIns()) {
      // Skip all InScalars
      if (In->getParent().get() != &Comp) continue;

      const std::string InName = getOpName(In);
      errs() << In->getUniqueName() << " from " << P->getUniqueName() << "\n";
      int InReady = getReadyCycle(InName);

      // Latency of input operation
      op_p Op = I.getOperatorForHW(InName);

      if (Op) {
        InReady += Op->Cycles;
      }

      MaxPreds = std::max(MaxPreds, InReady);
    }

    const std::string OpName = getOpName(P);

    ReadyMap[OpName] = MaxPreds;

    // Critical Path
    op_p Op = I.getOperatorForHW(OpName);

    if (Op)
      MaxPreds += Op->Cycles;

    CriticalPath = std::max((unsigned) MaxPreds, CriticalPath);
  }

  if (!NoPreserveLoadOrder) {
    StreamPort::LoadListTy LL = Comp.getLoads();

    for (StreamPort::LoadListTy::iterator LI = LL.begin(), LE = LL.end(); LI != LE; ++LI) {
      if (std::next(LI) == LE) break;

      loadaccess_p This = *LI;
      loadaccess_p Next = *(std::next(LI));

      AddedConsTy::iterator I = AddedCons.find(std::make_pair(This->getUID(), Next->getUID()));
      if (I != AddedCons.end()) {
        AddedCons.erase(I);
        This->delOut(Next);
        Next->delIn(This);
      }
    }
  } 

  ODEBUG("Critical path of " << Comp.getUniqueName() << ": " << CriticalPath);
}

int BlockModule::getReadyCycle(const std::string OpName) const {
  ReadyMapConstItTy E = ReadyMap.find(OpName);

  if (E == ReadyMap.end()) {
    ODEBUG("No Entry in ReadyMap for " << OpName);
  }

  assert(E != ReadyMap.end() && "No scheduling information");

  //errs() << OpName << ": " << E->second << "\n";

  return E->second;
}

void BlockModule::genTestBench() const {
  std::stringstream DoS;

  const std::string BName = Comp.getName();
  const std::string FileName = BName+"_tb.do";

  FileTy DoFile = openFile(FileName);

  DoS << "# " << BName << " testbench\n";
  DoS << "# run with 'vsim -do " << FileName << "'\n";
  DoS << "vlib work\n";

  for (const std::string &S : getFiles()) {
    if (ends_with(S, ".vhd")) {
      DoS << "vcom " << S << "\n";
    } else if (ends_with(S, ".v")) {
      DoS << "vlog -sv " << S << "\n";
    } else
      llvm_unreachable("Invalid filetype");
  }

  DoS << "vsim -novopt work." << BName << " -t {1 ns}\n";

  // clk and reset
  DoS << "force /" << BName << "/clk 0 0ns, 1 5ns -r 10ns\n";
  DoS << "force /" << BName << "/rst 1 0ns, 0 50ns\n";

  unsigned II = 0;
  // scalar inputs
  for (const scalarport_p P : Comp.getInScalars()) {
    if (P->isPipelined()) {
      // _unbuf, _unbuf_valid
      if (P->isFP())
        DoS << "force /" << BName << "/" << getOpName(P) << "_unbuf 2#" << flopoco::convert(0.5, 8, 23) << " 60ns\n";
      else
        DoS << "force /" << BName << "/" << getOpName(P) << "_unbuf 6#1 60ns\n";

      DoS << "force /" << BName << "/" << getOpName(P) << "_unbuf_valid 16#1 60ns\n";

      DoS << "when { /" << BName << "/" << getOpName(P) << "_ack==1} {\n";
      DoS << Indent(++II) << "noforce /" << BName << "/" << getOpName(P) << "_unbuf\n";
      DoS << Indent(II--) << "force /" << BName << "/" << getOpName(P) << "_unbuf_valid 0\n";
      DoS << "}\n";
    } else {
      if (P->isFP())
        DoS << "force /" << BName << "/" << getOpName(P) << " 2#" << flopoco::convert(0.5, 8, 23) << "\n";
      else
        DoS << "force /" << BName << "/" << getOpName(P) << " 16#1\n";
    }
  }
  // Load signals
  for (const loadaccess_p L : Comp.getLoads()) {
    DoS << "when { /" << BName << "/" << getOpName(L) << "_address_valid==1} {\n";
    if (L->getStream()->isFP())
      DoS << Indent(II+1) << "force /" << BName << "/" << getOpName(L) << "_unbuf 2#" << flopoco::convert(0.5, 8, 23) << " 30ns\n";
    else
      DoS << Indent(II+1) << "force /" << BName << "/" << getOpName(L) << "_unbuf 16#4 30ns\n";

    DoS << Indent(II+1) << "force /" << BName << "/" << getOpName(L) << "_unbuf_valid 1 30ns\n";
    DoS << "}\n";

    DoS << "when { /" << BName << "/" << getOpName(L) << "_ack==1} {\n";
    DoS << Indent(++II) << "force /" << BName << "/" << getOpName(L) << "_unbuf 0 15ns\n";
    DoS << Indent(II--) << "force /" << BName << "/" << getOpName(L) << "_unbuf_valid 0 15ns\n";
    DoS << "}\n";
  }

  for (const scalarport_p P : Comp.getOutScalars()) {
    DoS << "when { /" << BName << "/" << getOpName(P) << "_valid==1} {\n";
    DoS << Indent(II+1) << "force /" << BName << "/" << getOpName(P) << "_ack 1 20 ns\n";
    DoS << "}\n";
  }

  DoS << "add wave -r /" << BName << "/*\n";
  DoS << "property wave -radix hexadecimal /" << BName << "/*\n";

  DoS << "run 1 us\n";
  DoS << "seetime work 0\n";

  (*DoFile) << DoS.str();
  DoFile->close();
}


#ifdef DEBUG_TYPE
#undef DEBUG_TYPE
#endif
