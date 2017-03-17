include "llvm/Target/Target.td"

//Debugging Features
//name attribute, value, description, implied Features

//At the moment features are not used in any way. It has to be determined if an
//external HW-description suits better than using LLVM.

def virtex5 : SubtargetFeature <"Virtex5", 
  "Virtex5", "true", "Virtex5 FPGA", []>;

def stratix5 : SubtargetFeature <"Stratix5", 
  "Stratix5", "true", "Stratix5 FPGA", []>;

class FpgaBoard<string Name, list<SubtargetFeature> Features>
  : Processor<Name, NoItineraries, Features>;

def : FpgaBoard<"s5phq_d5",[stratix5]>;
def : FpgaBoard<"max2_22336b",[virtex5]>;

def OCLAcc : Target {
}
