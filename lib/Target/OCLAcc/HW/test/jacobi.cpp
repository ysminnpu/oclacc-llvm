#include <iostream>
#include <string>

#include "HW/hw.h"
#include "HW/factory.h"
#include "HW/streams.h"
#include "HW/operators.h"
#include "HW/visitor/dot.h"
#include "HW/visitor/latency.h"

#define SIZE_X 256
#define SIZE_Y 256
#define WIDTH 32

using namespace oclacc;

int main() {

  DotVisitor v("jacobi2D.dot");

  Kernel K("Jacobi2D");
    
  instream_p din = instream_p(new InStream(32, "data"));
  outstream_p dout = outstream_p(new OutStream(32, "result"));

  K.appIn(din);
  K.appOut(dout);

  base_p n = base_p(new Reg("n"));
  base_p e = base_p(new Reg("e"));
  base_p s = base_p(new Reg("s"));
  base_p w = base_p(new Reg("w"));
  base_p c = base_p(new Reg("c"));

  base_p fifo1 = base_p(new Fifo(SIZE_X-2, "fifo1"));
  base_p fifo2 = base_p(new Fifo(SIZE_X-2, "fifo2"));

  din > s > fifo1 > e > c > w > fifo2 > n;

  //base_p sum = F->sadd(32,0);

  //base_p div = F->ssub(32,0);

  base_p sum = base_p(new Tmp("sum"));
  base_p div = base_p(new Tmp("div"));

  n + e + s + w + c > sum;

  //sum / 5.0 > div;
  
  div > dout;


  if (K.accept(v)) {
    std::cout << "visitor failed" << std::endl;
  };

  std::cout << std::endl;

  LatencyVisitor latency;

  if (K.accept(latency) )
  {
    std::cout << "latency failed" << std::endl;
  };


  return 0;
}
