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

static cl::opt<bool> NoPreserveLoadOrder("no-preserve-load-order", cl::init(false), cl::desc("Keep the order of memory loads.") );

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

const std::string BlockModule::declConstValues() const {
  // Use a set to get unique definitions.
  std::unordered_set<std::string> SS;
  std::stringstream S;

  S << "// ConstVals\n";
  for (const const_p C : Comp.getConstVals()) {
    if (C->isStatic()) continue;
    Signal CS(getOpName(*C), C->getBitWidth(), Signal::Local, Signal::Reg);

    std::string Decl = CS.getDefStr() + " = 'b" + C->getBits();
    SS.insert(Decl);
  }

  for (const std::string &US : SS) {
    S << US << ";\n";
  }

  return S.str();
}

const std::string BlockModule::declFSMSignals() const {
  std::stringstream S;

  std::vector<std::string> States = {"state_free", "state_busy", "state_wait_load", "state_wait_store", "state_wait_barrier", "state_wait_output"};

  S << "// FSM signals\n";
  int N=0;
  for (const std::string ST : States) {
    S << "localparam "<< ST << "=" << N++ << ";\n";
  }

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

  S << "// Asynchronous state and output\n";
  S << "always @(*)" << "\n";
  S << "begin\n";

  // Make sure outputs are logik
  S << Indent(II) << "next_state <= state;\n";
  S << Indent(II) << "counter_enabled <= 0;\n";

  S << Indent(II) << "case (state)" << "\n";
  S << Indent(II) << "state_free:" << "\n";
  {
    BEGIN(S);

    S << Indent(II++) << "if(\n";

    Prefix = "";
    for (const std::string &N : ScalarInputNames) {
        S << Prefix << Indent(II) << N << "_valid" << " == 1";
      Prefix = " && \n";
    }
    S << "\n" << Indent(--II) << ")" << "\n";

    // All inputs are valid. If we only have a barrier, skip the busy state and
    // just jump to the barrier state.
    barrier_p B = Comp.getBarrier();
    if (B != nullptr)
      S << Indent(II) << "next_state <= state_wait_barrier;\n";
    else
      S << Indent(II) << "next_state <= state_busy" << ";\n";

    END(S);
  }
  S << Indent(II) << "state_busy:" << "\n";
  {
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

    END(S);
  }
  S << Indent(II) << "state_wait_output:" << "\n";
  {
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
  }

  S << Indent(II) << "state_wait_store:" << "\n";
  {
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
    } else
      S << Indent(II) << "// no stores\n";
    END(S);
  }
  S << Indent(II) << "state_wait_load:" << "\n";
  {
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
    } else
      S << Indent(II) << "// no loads\n";
    END(S);
  }
  // Each Barrier is a single input "..._release" to the Block, created by the top level
  // design.
  S << Indent(II) << "state_wait_barrier:" << "\n";
  {
    BEGIN(S);
    barrier_p B = Comp.getBarrier();
    if (B != nullptr) {
      const std::string BName = B->getUniqueName();
      S << Indent(II) << "if (" << BName << "_release == 1) next_state <= state_busy;\n";
    } else
      S << Indent(II) << "// no barriers\n";
    END(S);
  }

  S << Indent(II) << "endcase" << "\n";
  END(S);

  ////////////////////////////////////////////////////////////////////////////
  ////////////////////////////////////////////////////////////////////////////
  ////////////////////////////////////////////////////////////////////////////

  S << "// Synchronous next_state and register outputs\n";
  S << "always @(posedge clk)" << "\n";
  S << "begin\n";
  // Reset state
  S << Indent(II) << "if (rst)\n";
  {
    BEGIN(S);
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

    // Counter
    S << Indent(II) << "counter <= '0;\n";

    // Barrier
    barrier_p B = Comp.getBarrier();
    if (B != nullptr)
      S << Indent(II) << B->getUniqueName()<< "_reached <= 0;\n";

    END(S);
  }
  S << Indent(II) << "else\n";
  {
    BEGIN(S);
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
    {
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
    }

    // Barrier, reset output at once
    barrier_p B = Comp.getBarrier();
    if (B != nullptr) {
      S << Indent(II) << "if (" << B->getUniqueName() << "_release==1)\n";
      S << Indent(II+1) << B->getUniqueName() << "_reached <= 0;\n";
    }


    S << Indent(II) << "case (state)" << "\n";
    S << Indent(II) << "state_free:" << "\n";
    {
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
      END(S);
    }

    S << Indent(II) << "state_busy:" << "\n";
    {
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
    }
    
    S << Indent(II) << "state_wait_output:" << "\n";
    {
      BEGIN(S);
      END(S);
    }

    S << Indent(II) << "state_wait_store:" << "\n";
    {
      BEGIN(S);
      END(S);
    }

    S << Indent(II) << "state_wait_load:" << "\n";
    {
      BEGIN(S);
      END(S);
    }

    S << Indent(II) << "state_wait_barrier:" << "\n";
    {
      BEGIN(S);
      barrier_p B = Comp.getBarrier();
      if (B != nullptr)
        S << Indent(II) << B->getUniqueName()<< "_reached <= 1;\n";
      else
        S << "// no barrier\n";
      END(S);
    }


    S << Indent(II) << "endcase" << "\n";

    S << Indent(II) << "// Reset counter\n";
    S << Indent(II) << "if (next_state == state_free) counter <= '0;\n";
    END(S);
  }
  END(S);

  return S.str();
}

const std::string BlockModule::declStores() const {
  std::stringstream S;
  S << "// Store processes\n";

  for (storeaccess_p SA : Comp.getStores()) {
    assert(SA->getIns().size() == 2 && "Stores must have an index and value");

    const std::string Name = getOpName(SA);
    const streamindex_p Index = SA->getIndex();

    std::string IndexName;

    if (staticstreamindex_p SI = std::dynamic_pointer_cast<StaticStreamIndex>(Index)) {
      IndexName = SI->getIndex()->getUniqueName();
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
      IndexName = SI->getIndex()->getUniqueName();
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

#undef DEBUG_TYPE
#define DEBUG_TYPE "timing"

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

#undef DEBUG_TYPE
#define DEBUG_TYPE "verilog"

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

  barrier_p B = Comp.getBarrier();
  if (B != nullptr) {
    DoS << "when { /" << BName << "/" << B->getUniqueName() << "_reached==1} {\n";
    DoS << Indent(II+1) << "force /" << BName << "/" << B->getUniqueName() << "_release 1 20 ns\n";
    DoS << "}\n";
  }

  DoS << "add wave -r /" << BName << "/*\n";
  DoS << "property wave -radix hexadecimal /" << BName << "/*\n";

  DoS << "run 1 us\n";
  DoS << "seetime work 0\n";

  (*DoFile) << DoS.str();
  DoFile->close();
}
