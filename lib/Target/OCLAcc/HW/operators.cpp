#include "operators.h"

#include "HW.h"
#include "Streams.h"
#include "Memory.h"
#include "Arith.h"

using namespace oclacc;

// Operators

//base_p oclacc::operator>(input_p r0, base_p r1)
//{
//  r0->appOut(r1);
//  r1->appIn(r0);
//  return r1;
//}
//void oclacc::operator>(base_p r0, outstream_p r1)
//{
//  r0->appOut(r1);
//  r1->appIn(r0);
//}

//base_p oclacc::operator>(base_p r0, base_p r1)
//{
//  r0->appOut(r1);
//  r1->appIn(r0);
//  return r1;
//}

//base_p oclacc::operator>(base_p r0, size_t delay)
//{
//  base_p head = r0;
//  for (size_t i = 0; i < delay; i++)
//  {
//    std::stringstream Name;
//    Name << "Reg" << i;
//    base_p reg = reg_p(new Reg(Name.str()));
//    head->appOut(reg);
//    reg->appIn(head);
//
//    head = reg;
//  }
//  return head;
//}
//
//base_p oclacc::operator+(base_p r0, base_p r1)
//{
//  base_p op = base_p( new Add("add") );
//  r0->appOut(op);
//  r1->appOut(op);
//
//  op->appIn(r0);
//  op->appIn(r1);
//
//  return op;
//}
//base_p oclacc::operator-(base_p r0, base_p r1)
//{
//  base_p op = base_p( new Sub("sub") );
//  r0->appOut(op);
//  r1->appOut(op);
//
//  op->appIn(r0);
//  op->appIn(r1);
//
//  return op;
//}
//
//base_p oclacc::operator*(base_p r0, base_p r1)
//{
//  base_p op = base_p( new Mul("mul") );
//  r0->appOut(op);
//  r1->appOut(op);
//
//  op->appIn(r0);
//  op->appIn(r1);
//
//  return op;
//}
//base_p oclacc::operator/(base_p r0, base_p r1)
//{
//  base_p op = base_p( new Div("div") );
//  r0->appOut(op);
//  r1->appOut(op);
//
//  op->appIn(r0);
//  op->appIn(r1);
//
//  return op;
//}
