#ifndef RAM_H
#define RAM_H

#include "Macros.h"

#include <sstream>
#include <cmath>

namespace oclacc {

/// \brief BlockRAM with \param Size in bytes
///
/// We need no definition because we use an IP block.
class BRamImpl {
  private:
    unsigned BitWidth;
    unsigned Size;
    std::string InstName;

  public:
    BRAM(std::string Name, unsigned Size) : BitWidht(S->getBitWidth()), Size(Size), InstName(Name) {
    }

    std::string instantiate() {
      std::stringstream S;

      // Calc Address wisth and round up if size is not a power of 2.
      unsigned AddrWidth = log2(Size / BitWdith);
      if (pow(2,AddrWidth) < Size) 
        AddrWidth++;

      S << "bram #(\n";
      S << I(1) << "DATA(" << BitWidth << "),\n";
      S << I(1) << "ADDR(" << AddrWidth << ")\n";
      S << ")" << InstName << "(\n";

      S << I(1) << "// Port A\n";
      S << I(1) << "a_clk(clk)\n";
      S << I(1) << "a_wr(" << InstName << "_a_wr),\n";
      S << I(1) << "a_addr(" << InstName << "_a_addr),\n";
      S << I(1) << "a_din(" << InstName << "_a_din),\n";
      S << I(1) << "a_dout(" << InstName << "_a_dout),\n";

      S << I(1)  << "// Port B\n";
      S << I(1) << "b_clk(" << InstName << "clk),\n";
      S << I(1) << "b_wr(" << InstName << "_b_wr),\n";
      S << I(1) << "b_addr(" << InstName << "_b_addr),\n";
      S << I(1) << "b_din(" << InstName << "_b_din),\n";
      S << I(1) << "b_dout(" << InstName << "_b_dout)\n";
      S << ");\n";

      return S.str();
    }

    std::string define() {
      std::string R = R"BLOCK(
// http://danstrother.com/2010/09/11/inferring-rams-in-fpgas/
// A parameterized, inferable, true dual-port, dual-clock block RAM in Verilog.

module bram #(
    parameter DATA = 72,
    parameter ADDR = 10
) (
    // Port A
    input   wire                a_clk,
    input   wire                a_wr,
    input   wire    [ADDR-1:0]  a_addr,
    input   wire    [DATA-1:0]  a_din,
    output  reg     [DATA-1:0]  a_dout,

    // Port B
    input   wire                b_clk,
    input   wire                b_wr,
    input   wire    [ADDR-1:0]  b_addr,
    input   wire    [DATA-1:0]  b_din,
    output  reg     [DATA-1:0]  b_dout
);

// Shared memory
reg [DATA-1:0] mem [(2**ADDR)-1:0];

// Port A
always @(posedge a_clk) begin
    a_dout      <= mem[a_addr];
    if(a_wr) begin
        a_dout      <= a_din;
        mem[a_addr] <= a_din;
    end
end

// Port B
always @(posedge b_clk) begin
    b_dout      <= mem[b_addr];
    if(b_wr) begin
        b_dout      <= b_din;
        mem[b_addr] <= b_din;
    end
end

endmodule)BLOCK"
    return R;
}

/// \brief Instantiate BlockRAM and a frontent queue to support read/write
/// synchronization for multiple access to a limited number of ports.
///
class BRam {
  public:
    BRam(streamport_p S) {
    }

    std::string instantiate() {
      std::stringstream S;

      return S.str();
    }
};

} // end ns oclacc

#endif /* RAM_H */
