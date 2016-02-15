#ifndef TYPEDEFS_H
#define TYPEDEFS_H

#include <memory>

namespace oclacc {

class HW;
typedef std::shared_ptr<HW> base_p;

class InScalar;
typedef std::shared_ptr<InScalar> inscalar_p;

class InStream;
typedef std::shared_ptr<InStream> instream_p;

class OutScalar;
typedef std::shared_ptr<OutScalar> outscalar_p;

class OutStream;
typedef std::shared_ptr<OutStream> outstream_p;

class Input;
typedef std::shared_ptr<Input> input_p;

class Output;
typedef std::shared_ptr<Output> output_p;

class Stream;
typedef std::shared_ptr<Stream> stream_p;

class Kernel;
typedef std::shared_ptr<Kernel> kernel_p;

class Block;
typedef std::shared_ptr<Block> block_p;

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

class And;
typedef std::shared_ptr<And> and_p;

class Or;
typedef std::shared_ptr<Or> or_p;

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

//class DataType;
//typedef std::shared_ptr<DataType> type_p;
//

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
