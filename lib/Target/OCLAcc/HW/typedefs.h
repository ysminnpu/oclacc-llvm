#ifndef TYPEDEFS_H
#define TYPEDEFS_H

#include <vector>

#include <memory>

namespace oclacc {

class HW;
typedef std::shared_ptr<HW> base_p;

class FPHW;
typedef std::shared_ptr<FPHW> basefp_p;

class Port;
typedef std::shared_ptr<Port> port_p;

class ScalarPort;
typedef std::shared_ptr<ScalarPort> scalarport_p;

class StreamPort;
typedef std::shared_ptr<StreamPort> streamport_p;

class Kernel;
typedef std::shared_ptr<Kernel> kernel_p;

class Block;
typedef std::shared_ptr<Block> block_p;

class Component;
typedef std::shared_ptr<Component> component_p;

class Arith;
typedef std::shared_ptr<Arith> arith_p;

class FPArith;
typedef std::shared_ptr<FPArith> fparith_p;

class Add;
typedef std::shared_ptr<Add> add_p;

class Sub;
typedef std::shared_ptr<Sub> sub_p;

class Mul;
typedef std::shared_ptr<Mul> mul_p;

class Div;
typedef std::shared_ptr<Div> div_p;

class Rem;
typedef std::shared_ptr<Rem> rem_p;

class Shl;
typedef std::shared_ptr<Shl> shl_p;

class LShr;
typedef std::shared_ptr<LShr> lshr_p;

class AShr;
typedef std::shared_ptr<AShr> ashr_p;

class And;
typedef std::shared_ptr<And> and_p;

class Or;
typedef std::shared_ptr<Or> or_p;

class Xor;
typedef std::shared_ptr<Xor> xor_p;

class Mux;
typedef std::shared_ptr<Mux> mux_p;

class Reg;
typedef std::shared_ptr<Reg> reg_p;

class Ram;
typedef std::shared_ptr<Ram> ram_p;

class Fifo;
typedef std::shared_ptr<Fifo> fifo_p;

class ConstVal;
typedef std::shared_ptr<ConstVal> const_p;

class Compare;
typedef std::shared_ptr<Compare> cmp_p;

class StreamIndex;
typedef std::shared_ptr<StreamIndex> streamindex_p;

class DynamicStreamIndex;
typedef std::shared_ptr<DynamicStreamIndex> dynamicstreamindex_p;

class StaticStreamIndex;
typedef std::shared_ptr<StaticStreamIndex> staticstreamindex_p;

} //end namespace oclacc

#endif /* TYPEDEFS_H */
