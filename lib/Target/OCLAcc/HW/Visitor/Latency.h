#ifndef LATENCY_H
#define LATENCY_H

#include "DFVisitor.h"

namespace oclacc {

class LatencyVisitor : public DFVisitor
{
  private:
    typedef DFVisitor super;

  public:
    uint32_t latency;

    LatencyVisitor() : latency(0)
    {
      //pass
    }
    ~LatencyVisitor() 
    {
      std::cout << "latency: " << latency << std::endl;  
    }

    virtual int visit(Reg &r)
    {
  //    VISIT_ONCE;
      std::cout << "visit(Reg)" << std::endl;

      latency++;

      super::visit(r);

      return 0;
    }

    virtual int visit(Fifo &r)
    {
  //    VISIT_ONCE;
      std::cout << "visit FiFo" << std::endl;

      latency+=r.D;
      
      super::visit(r);

      return 0;
    }

};

}
#endif /* LATENCY_H */
