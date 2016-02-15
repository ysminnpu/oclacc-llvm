#include "VhdlGen.h"

namespace oclacc {

VhdlEntity::VhdlEntity(const std::string &Name,
        port_p Clk,
        port_p Reset )
      : Identifiable(Name), VhdlWriteable(Name),
        Clk( Clk ), Reset( Reset ) {

        }

} // end namespace oclacc
