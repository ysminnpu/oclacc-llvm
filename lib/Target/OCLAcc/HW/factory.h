#ifndef FACTORY_H
#define FACTORY_H

#include "hw.h"
#include "typedefs.h"
#include "arith.h"
#include "const.h"
#include "control.h"
#include "kernel.h"
#include "mem.h"
#include "streams.h"

#include <stdio.h>

namespace oclacc {

class KernelFactory
{
  private:
    KernelFactory( const KernelFactory &o ) : Kernel( o.Kernel ) {
      std::cout << "COPY KERNEL" << std::endl;
    }
  public:
    kernel_p Kernel;

    KernelFactory() {
      std::cout << "CREATE FACTORY DEFAULT" << std::endl;
    }

    KernelFactory( std::string name ) : Kernel( new class Kernel( name ) )
    {
      if ( ! name.size() )
        std::cerr << "KernelName must not be empty" << std::endl;

      std::cout << "CREATE FACTORY" << std::endl;
    }


    kernel_p getKernel() {
      return Kernel;
    }

    add_p add()
    {
      return add_p(new Add("Add"));
    }
    sub_p sub()
    {
      return sub_p(new Sub("Sub"));
    }
    mul_p mul()
    {
      return mul_p(new Mul("Mul"));
    }
    div_p div()
    {
      return div_p(new Div("Div"));
    }

    mux_p mux(base_p s) 
    {
      mux_p m = mux_p(new Mux(s, "Mux"));
      //s->appSink(m);
      return m;
    }

    inscalar_p inscalar(size_t W, std::string Name)
    {
      inscalar_p s(new InScalar(W,Name));
      Kernel->appIn(s);
      return s;
    }

    instream_p instream(size_t W, std::string Name)
    {
      instream_p s = instream_p(new InStream(W,Name));
      Kernel->appIn(s);
      return s;
    }

    outscalar_p outscalar(size_t W, std::string Name)
    {
      outscalar_p s = outscalar_p(new OutScalar(W,Name));
      Kernel->appOut(s);
      return s;
    }
    outstream_p outstream(size_t W, std::string Name)
    {
      outstream_p s = outstream_p(new OutStream(W,Name));
      Kernel->appOut(s);
      return s;
    }

    reg_p reg()
    {
      reg_p r = reg_p(new Reg("Reg"));
      return r;
    }

    ram_p ram(base_p a, size_t W, size_t D)
    {
      ram_p r = ram_p(new Ram(a, W, D, "Ram"));
      return r;
    }

    fifo_p fifo(size_t D)
    {
      fifo_p r = fifo_p(new Fifo(D, "Fifo"));
      return r;
    }

    const_p sconst(int64_t V)
    {
      hwtype_p T = hwtype_p(new HWType() );
      const_p r = const_p(new ConstVal(T, V));
      Kernel->appConst(r);
      return r;
    }
    const_p uconst(uint64_t V)
    {
      hwtype_p T = hwtype_p(new HWType() );
      const_p r = const_p(new ConstVal(T, V));
      Kernel->appConst(r);
      return r;
    }
    const_p fconst(double V)
    {
      hwtype_p T = hwtype_p(new HWType() );
      uint64_t IV = (uint64_t) V;
      const_p r = const_p(new ConstVal(T, IV));
      Kernel->appConst(r);
      return r;
    }

    base_p tmp()
    {
      base_p r = base_p(new Tmp("Tmp"));
      return r;
    }
    base_p tmp(std::string &name)
    {
      base_p r = base_p(new Tmp(name));
      return r;
    }

};

}

#endif /* FACTORY_H */
