#include <memory>
#include <sstream>
#include <cmath>
#include <set>

#include "../../HW/Kernel.h"

#include "Verilog.h"
#include "VerilogModule.h"
#include "Naming.h"
#include "VerilogMacros.h"

#define BEGIN {S << Indent(++II) << "begin\n";}while(0);
#define END {S << Indent(II--) << "end\n";}while(0);


using namespace oclacc;

extern DesignFiles TheFiles;

VerilogModule::VerilogModule(Component &C) : Comp(C) {
}

VerilogModule::~VerilogModule() {
}

const std::string VerilogModule::declFooter() const {
  std::stringstream S;
  S << "endmodule " << " // " << Comp.getUniqueName() << "\n";
  return S.str();
}


BlockModule::BlockModule(Block &B) : VerilogModule(B), Comp(B) {
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
const std::string KernelModule::declBlockWires() const {
  std::stringstream S;

  for (block_p B : Comp.getBlocks()) {
    S << "// " << B->getUniqueName() << "\n";

    // Create wires for each port used as input and output and assign them to
    // their use in a component
    for (scalarport_p P : B->getInScalars()) {
      // Non-pipelined Inputs are the same as the Kernel's.
      if (!P->isPipelined())
        continue;

      // Make sure that the Block's ScalarInput is connected with the Kernel's
      HW::PortsSizeTy NumIns = P->getIns().size();
      assert(NumIns);

      if (NumIns == 1) {
        scalarport_p SI = std::static_pointer_cast<ScalarPort>(P->getIn(0));

        Signal::SignalListTy KernelSigs = getOutSignals(SI);
        Signal::SignalListTy BlockSigs = getInSignals(P);

        assert(KernelSigs.size() == BlockSigs.size());


        for (Signal::SignalListConstItTy
            KI = KernelSigs.begin(),
            BI = BlockSigs.begin(),
            KE = KernelSigs.end();
            KI != KE;
            ++KI, ++BI) {

          // Define Block's Port. All Ports are wires.
          Signal LocDef(BI->Name, BI->BitWidth, Signal::Local, Signal::Wire);
          S << LocDef.getDefStr() << ";\n";

          // Kernel->Block
          if (KI->Direction == Signal::Out) {
            S << "assign " << BI->Name << " = " << KI->Name << ";\n"; 
          } else if (KI->Direction == Signal::In) {
            S << "assign " << KI->Name << " = " << BI->Name << ";\n"; 
          }
        }
      } else 
        TODO("Add Muxer");
    }
  }
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

const std::string BlockModule::declConstValues() const {
  // Use a set to get unique definitions.
  std::set<std::string> SS;
  std::stringstream S;

  S << "// ConstVals\n";
  for (const const_p C : Comp.getConstVals()) {
    Signal CS(getOpName(*C), C->getBitWidth(), Signal::Local, Signal::Reg);
    SS.insert(CS.getDefStr());
  }

  for (const std::string &US : SS) {
    S << US << ";\n";
  }

  return S.str();
}

const std::string BlockModule::declFSMSignals() const {
  std::stringstream S;

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

  // All _valid signals of Inputs
  std::vector<std::string> InputNames;

  for (scalarport_p P : Comp.getInScalars()) {
    if (P->isPipelined())
      InputNames.push_back(getOpName(P));
  }
  for (streamindex_p P : Comp.getInStreamIndices()) {
    InputNames.push_back(getOpName(P));
  }

  // And of Outputs
  std::vector<std::string> OutputNames;
  for (scalarport_p P : Comp.getOutScalars()) {
    if (P->isPipelined())
      InputNames.push_back(getOpName(P));
  }
  for (streamindex_p P: Comp.getOutStreamIndices()) {
    OutputNames.push_back(getOpName(P));
  }

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
    for (const std::string &N : InputNames) {
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
    for (streamindex_p SI : Comp.getOutStreamIndices()) {
      const std::string Name = getOpName(SI);
      unsigned C = getReadyCycle(Name);

      S << Indent(II++) << "if (counter == " << C << ")\n";
      S << Indent(II--) << "next_state <= state_wait_store;\n";
    }

    // state_wait_output
    S << Indent(II) << "if (counter == " << CriticalPath <<") next_state <= state_wait_output;\n";

    // state_

    S << Indent(II--) << "end" << "\n";

  S << Indent(II) << "state_wait_output:" << "\n";
    S << Indent(++II) << "begin" << "\n";
    
    S << Indent(II) << "counter_enabled <= 0;\n";

    // When all outputs and stores are acknoledged, return to state_free
    Prefix = "";
    S << Indent(II++) << "if (\n";
    for (const std::string N : OutputNames) {
      S << Prefix << Indent(II) << N << "_fin == 1";
      Prefix = " &&\n";
    }
    S << "\n" << Indent(II) << ")\n";
    S << Indent((II--)+1) << "next_state <= state_free;\n";

    S << Indent(II--) << "end" << "\n";

  S << Indent(II) << "state_wait_store:" << "\n";
    S << Indent(++II) << "begin" << "\n";
    S << Indent(II--) << "end" << "\n";

  S << Indent(II) << "state_wait_load:" << "\n";
    S << Indent(++II) << "begin" << "\n";
    S << Indent(II--) << "end" << "\n";


  S << Indent(II) << "endcase" << "\n";
  S << "end\n";


  // Synchronous next_state
  S << "always @(posedge clk)" << "\n";
  S << "begin\n";
  // Reset state
  S << Indent(II) << "if (rst)\n";
    S << Indent(++II) << "begin\n";
    S << Indent(II) << "state <= state_free;" << "\n";

    // Input buffers and valids
    for (const std::string &N : InputNames) {
      S << Indent(II) << N << " <= '0;\n";
      S << Indent(II) << N << "_valid <= 0" << ";\n";
    }

    // Outout buffers and valids
    for (const std::string &N : OutputNames) {
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
    for (const std::string &N : InputNames) {
      S << Indent(II) << N << "_unbuf_ack <= 0;\n";
    }

    // Reset status signals in state_free
    S << Indent(II) << "if (state == state_free)\n";
    BEGIN;
    for (const std::string &N : OutputNames) {
      S << Indent(II) << N << "_fin <= 0;\n";
    }
    END;

    // Allow others to acknowledge an output port in every state except state_free
    S << Indent(II) << "if (state != state_free)\n";
    BEGIN;
    for (const std::string &N : OutputNames) {
      S << Indent(II) << "if (" << N << "_ack == 1) " << N << "_fin <= 1;\n";
    }
    END;

    S << Indent(II) << "case (state)" << "\n";
    S << Indent(II) << "state_free:" << "\n";
      BEGIN;

      // Buffer Inputs

      for (const std::string &N : InputNames) {
        S << Indent(II) << "if (" << N << "_unbuf_valid" << ")\n";
        BEGIN;
        S << Indent(II) << N << "_unbuf_ack" << " <= 1;\n";
        S << Indent(II) << N << " <= " << N << "_unbuf;\n";
        S << Indent(II) << N << "_valid" << " <= 1;\n";
        END;
      }
      S << Indent(II--) << "end" << "\n";

    S << Indent(II) << "state_busy:" << "\n";
      BEGIN;
      S << Indent(II) << "if (counter_enabled)\n";
        BEGIN;
        S << Indent(II+1) << "if (counter < " << CriticalPath << ") counter <= counter + 1;\n";
        S << Indent(II+1) << "else counter <= '0;\n";
        END;

        // Output gets ready, set valid
        for (const std::string &N : OutputNames) {
          unsigned C = getReadyCycle(N);

          S << Indent(II) << "if (counter == " << C << ")\n";
            BEGIN;
            S << Indent(II) << N << "_valid <= 1;\n";
            END;
        }
      END;
    
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
    END;
  END;

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

  S << "// OutScalar fin\n";
  for (const scalarport_p P : Comp.getOutScalars()) {
    Signal SP(getOpName(P), P->getBitWidth(), Signal::Local, Signal::Reg);
    S << SP.getDefStr() << ";\n";

    Signal SF(getOpName(P)+"_fin", 1, Signal::Local, Signal::Reg);
    S << SF.getDefStr() << ";\n";
  }

  S << "// OutStream fin\n";
  for (const streamindex_p P : Comp.getOutStreamIndices()) {
    Signal SF(getOpName(P)+"_fin", 1, Signal::Local, Signal::Reg);
    S << SF.getDefStr() << ";\n";
  }

  return S.str();
}

void BlockModule::schedule(const OperatorInstances &I) {
  CriticalPath = 5;
}

unsigned BlockModule::getReadyCycle(const std::string OpName) const {
  return 5;
}

