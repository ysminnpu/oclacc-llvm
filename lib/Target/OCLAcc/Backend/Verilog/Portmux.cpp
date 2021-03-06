#include "Portmux.h"

#include <set>
#include <sstream>

#include "../../Utils.h"
#include "../../HW/Port.h"
#include "FileHeader.h"

#define I(C) std::string((C*2),' ')

using namespace oclacc;

std::set<std::string> ModInstances;

Portmux::Portmux(const Port &P) : InPort(P) {
  NumPorts = InPort.getIns().size();
  Bitwidth = InPort.getBitWidth();

  std::stringstream SS;
  SS << "portmux_" << NumPorts << "_" << Bitwidth;
  ModName = SS.str();

  FileName = ModName+".v";

  InstName = "portmux_"+InPort.getUniqueName();

  definition();
}

void Portmux::definition() {
  // Create definition file only once for each module
  //
  if (ModInstances.find(ModName) != ModInstances.end()) return;

  // Module was not created yet, so do it now.
  ModInstances.insert(ModName);


  FileTy F = openFile(FileName);

  std::stringstream S;

  S << header();

  S << "module " << ModName << "(\n";

  std::stringstream BW;
  if (Bitwidth > 1) {
    BW << "[" << Bitwidth-1 << ":0]";
  }
  std::string BWS = BW.str();

  for (unsigned i = 0; i < NumPorts; ++i) {
    S << I(1) << "input  wire " << BWS << " in" << i << ",\n";
    S << I(1) << "input  wire " << std::string(BWS.length()+1, ' ') << "in" << i << "_valid,\n";
    S << I(1) << "output wire " << std::string(BWS.length()+1, ' ') << "out" << i << "_ack,\n";
    S << I(1) << "//\n";
  }
  S << I(1) << "output wire " << BW.str() << " out," << "\n";
  S << I(1) << "input  wire " << std::string(BWS.length()+1, ' ') << "ack\n";
  S << ");\n";
  S << "\n";
  S << "reg " << BW.str() << " out_int;\n";
  S << "\n";

  // Implementation
  S << "always @*\n";
  S << "begin\n";
  S << I(1) << "out_int = 1'b0;\n";
  for (unsigned i = 0; i < NumPorts; ++i) {
    S << I(1);
    if (i != 0) 
      S << "else ";
    S << "if(in" << i << "_valid == 1)\n";
    S << I(2) << "out_int = in" << i << ";\n";
  }
  S << "end\n";
  S << "\n";
  S << "assign out = out_int;\n";
  S << "\n";
  S << "endmodule // " << ModName << "\n";

  (*F) << S.str();

  F->close();
}

std::string Portmux::instantiate() {
  std::stringstream S;

  S << ModName << " " << InstName << " (\n";

  int i = 0;
  for (base_p P : InPort.getIns()) {
    S << I(1) << ".in" << i << "(" << P->getUniqueName() << "),\n";
    S << I(1) << ".in" << i << "_valid(" << P->getUniqueName() << "_valid),\n";
    S << I(1) << ".out" << i << "_ack(" << P->getUniqueName() << "_ack),\n";
    ++i;
  }
  S << I(1) << ".out(" << InPort.getUniqueName() << ")\n";

  S << ");\n";

  return S.str();
}
