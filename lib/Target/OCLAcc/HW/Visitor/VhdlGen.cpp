#include "VhdlGen.h"

namespace oclacc {

namespace vhdl {

VhdlEntity::VhdlEntity(const std::string &Name,
        port_p Clk,
        port_p Reset )
      : Identifiable(Name), VhdlWriteable(Name),
        Clk( Clk ), Reset( Reset ) {

        }

} // end namespace vhdl
} // end namespace oclacc
