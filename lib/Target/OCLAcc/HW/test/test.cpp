#include <iostream>

#include "HW/hw.h"
#include "HW/factory.h"
#include "HW/visitor/dot.h"

int main() {

  DotVisitor v("kernel.dot");

  factory_p F(new KernelFactory("test"));

  instream_p din = F->instream(32, "data");
  outstream_p dout = F->outstream(32, "result");

  base_p a0 = F->add(); 
  base_p a1 = F->add(); 
  base_p a2 = F->add(); 
  base_p a3 = F->sub(); 
  base_p a4 = F->sub(); 
  base_p a5 = F->sub(); 

  base_p mux  = F->mux(a0);

  base_p ram = F->ram(a0, 32,1024);

  base_p fifo = F->fifo(256);

  din > a0 > a1 > 2 > mux;
  din > a2 > a3 > 2 > mux;
  din > a4;
  mux > a5 > ram > fifo > dout;

  if (F->getKernel()->accept(v)) {
    std::cout << "visitor failed" << std::endl;
  };

  return 0;
}

/* vim: set ts=2 sw=2 tw=80 ft=cpp et :*/
