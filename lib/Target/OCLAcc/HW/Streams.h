#ifndef STREAMS_H
#define STREAMS_H

#include "typedefs.h"
#include "Identifiable.h"
#include <unordered_set>

namespace oclacc {

class InScalar;

class Input : public HW
{
  public:
    virtual bool isScalar() = 0;

  protected:
    Input (const std::string &Name, size_t W) : HW(Name, W)
    {
    }

  private:
//    using HW::appIn;
};

class InScalar : public Input
{
  public:
    InScalar (const std::string &Name, size_t W) : Input(Name, W)
    {
    }

    bool isScalar() {
      return true;
    }

    DECLARE_VISIT


};

class Output : public HW
{

//  private:
//    using HW::appOut;

  protected:
    Output (const std::string &Name, size_t W) : HW(Name, W) { }


  public:
    Output(const Output&) = delete;
    Output& operator=(const Output&) = delete;

    virtual bool isScalar() = 0;

    virtual void appIn(base_p P) {
      In=P;
    }

    base_p getIn() const {
      return In;
    }

  protected:
    base_p In;

  private:
    using HW::getIn;
    using HW::getIns;

};

class OutScalar : public Output
{
  public:
    OutScalar (const std::string &Name, size_t W ) : Output(Name, W)
    {
    }

    OutScalar(const OutScalar&) = delete;
    OutScalar& operator=(const OutScalar&) = delete;

    bool isScalar() { return true; }

    DECLARE_VISIT

};

class Stream : public HW {
  private:
    std::unordered_set<streamindex_p> Indices;

  protected:
    Stream(const std::string &Name, size_t W) : HW(Name, W) { }

  public:
    Stream(const Stream &) = delete;
    Stream& operator=(const Stream&) = delete;

    bool isScalar() { return false; }

    virtual bool isInStream() = 0;
    virtual bool isOutStream() = 0;

    void appIndex(streamindex_p Index) {
      Indices.insert(Index);
    }

    const std::unordered_set<streamindex_p> &getIndices() const {
      return Indices;
    }

    ///
    // hasIndex - Return true if Stream already contains given Index 
    //
    bool hasIndex(streamindex_p I) const {
      return (Indices.count(I) != 0);
    }

    DECLARE_VISIT;
};

class StreamIndex : public HW {
  private:
    stream_p Stream;
    base_p In;

  protected:
    StreamIndex(const std::string &Name, stream_p Stream) : HW(Name,0), Stream(Stream) { }

  public:
    StreamIndex(const StreamIndex&) = delete;
    StreamIndex& operator=(const StreamIndex&) = delete;

    stream_p getStream() const {
      return Stream;
    }

    virtual bool isStatic() const = 0;
    DECLARE_VISIT;

};

class DynamicStreamIndex : public StreamIndex {

  private:
    base_p Index;

  public:
    DynamicStreamIndex(const std::string &Name, stream_p Stream, base_p Index) : StreamIndex(Name, Stream), Index(Index) { }

    DynamicStreamIndex(const DynamicStreamIndex&) = delete;
    DynamicStreamIndex& operator=(const DynamicStreamIndex&) = delete;


    virtual void appIndex(base_p I) {
      Index = I;
    }

    base_p getIndex() const { return Index; }

    bool isStatic() const {return false;}

    DECLARE_VISIT;
};

class StaticStreamIndex : public StreamIndex {
  private:
    size_t Index;
  public:
    StaticStreamIndex(const std::string &Name, stream_p Stream, size_t Index, size_t W) : StreamIndex(Name, Stream), Index(Index) {
      setBitwidth(W);
    }

    StaticStreamIndex(const StaticStreamIndex&) = delete;
    StaticStreamIndex& operator=(const StaticStreamIndex&) = delete;

    virtual void appIndex(size_t I) {
      Index = I;
    }
    size_t getIndex() const { return Index; }

    bool isStatic() const {return true;}

    const std::string getUniqueName() const {
      return getName();
    }

    DECLARE_VISIT;
};

class InStream : public Stream
{
  public:

    InStream (const std::string &Name, size_t W) : Stream(Name, W) { }
    InStream(const InStream&) = delete;
    InStream& operator=(const InStream&) = delete;

    bool isInStream() { return true; }
    bool isOutStream() { return false; }

    DECLARE_VISIT;
};

class OutStream : public Stream
{
  public:
    OutStream (const std::string &Name, size_t W) : Stream(Name, W) { }
    OutStream(const OutStream&) = delete;
    OutStream& operator=(const OutStream&) = delete;

    bool isInStream() { return false; }
    bool isOutStream() { return true; }

    DECLARE_VISIT;
};


#if 0
class InOutStream : public InStream, public OutStream
{

  public:
    using HW::appOut;

    InOutStream (const std::string &Name, size_t W ) : HW(Name), InStream(Name, W), OutStream(Name, W)
    {
    }

    virtual void appIn(base_p p) {
      In=p;
    }
    DECLARE_VISIT;
};
#endif


#if 0
class OutStreamIndex : public StreamIndex {
  private:
    virtual void appOut(base_p O) {};

  
  public:
    outstream_p Out;
    base_p Idx;
    base_p In;

    OutStreamIndex(const std::string &Name, base_p Idx ) : StreamIndex(Name), Idx(Idx) { }

    OutStreamIndex(const OutStreamIndex&) = delete;
    OutStreamIndex& operator=(const OutStreamIndex&) = delete;

    virtual void appIn( base_p I ) {
      In = I;
    }; 

    void appOut(outstream_p O) {
      Out = O;
    }

    DECLARE_VISIT;
};

class InStreamIndex : public StreamIndex {
  private:
    using HW::appIn;

  public:

    instream_p In;
    base_p Idx;

    InStreamIndex (const std::string &Name, instream_p In, base_p Idx ) : StreamIndex(Name), In(In), Idx(Idx)
    {
    }
    DECLARE_VISIT

  private:
    void appIn( base_p p ) {}; 
};
#endif

}

#endif /* STREAMS_H */
