#ifndef VHDL_KERNEL_H
#define VHDL_KERNEL_H

#include <fstream>
#include <iostream>
#include <cxxabi.h>
#include <typeinfo>
#include <sstream>
#include <memory>

#include "llvm/Support/Debug.h"

#include "../../todo.h"
#include "BFVisitor.h"
#include "VhdlGen.h"

#define TYPENAME(x) abi::__cxa_demangle(typeid(x).name(),0,0,NULL)
#define DEBUG_CALL DEBUG_WITH_TYPE( "VhdlVisitor", llvm::dbgs() << __PRETTY_FUNCTION__ << "\n" );

namespace oclacc {
namespace vhdl {

class VhdlVisitor : public BFVisitor
{
  private:
    typedef BFVisitor super;

    std::shared_ptr<VhdlEntity> Design;
    DesignUnit *CurrentDesign;

    port_p DesignReset;
    port_p DesignClk;

    std::shared_ptr<VhdlKernel> CurrentKernelEntity;
    Kernel *CurrentKernel;

    std::map< HW *, std::shared_ptr<VhdlEntity> > HWMap;
    std::map< HW *, signal_p> SignalMap;

  public:
    VhdlVisitor()
    {
      DEBUG_CALL;
    }

    ~VhdlVisitor()
    {
      DEBUG_CALL;
    }

    VhdlVisitor(const VhdlVisitor&) = delete;
    VhdlVisitor &operator =(const VhdlVisitor &) = delete;

    //
    //VISIT METHODS
    //

    virtual int visit(DesignUnit &r)
    {
      VISIT_ONCE(r);
      DEBUG_CALL;

      const std::string Name = r.getName()+"_Top";

      DesignClk = std::make_shared<VhdlInPort>( Name+"_Clk" );
      DesignReset = std::make_shared<VhdlInPort>( Name+"_Reset" );

      Design = std::make_shared<VhdlEntity>( Name, DesignClk, DesignReset);

      CurrentDesign = &r;

      super::visit(r);

      // Assign Clk,Rst signals to each Kernel.
      // TODO use different clock domains if needed
      for ( const std::shared_ptr<VhdlEntity> D : Design->getComponents() ) {
        Design->addSignalAssignment(D->getClk(), DesignClk);
        Design->addSignalAssignment(D->getReset(), DesignReset);
      }

      return 0;
    }

    virtual int visit(Kernel &r)
    {
      VISIT_ONCE(r);
      DEBUG_CALL;

      CurrentKernel = &r;

      const std::string Name = r.getName()+"_Kernel";

      auto Reset = std::make_shared<VhdlInPort>( Name+"_Reset" );
      auto Clk = std::make_shared<VhdlInPort>( Name+"_Clk" );

      CurrentKernelEntity = std::make_shared<VhdlKernel>( Name, Clk, Reset );
      Design->addComponent(CurrentKernelEntity);

      // OutStreams are handled by their visit functions since their signals are
      // mapped to the index instead of the stream.
      // 
      // Scalars have to be declared here, otherwise we have no access to the
      // shared_ptr keeping it.
      for ( scalarport_p p : r.getInScalars() ) {
        auto S = std::make_shared<VhdlInPort>( p->getUniqueName(), p->getBitwidth() );
        CurrentKernelEntity->InScalar.push_back( S );
        SignalMap[p.get()] = S;
      }

      for ( scalarport_p p : r.getOutScalars() ) {
        auto S = std::make_shared<VhdlInPort>( p->getUniqueName(), p->getBitwidth() );
        CurrentKernelEntity->InScalar.push_back(S);
        SignalMap[p.get()] = S;
      }

      //Visit in, out, constants
      super::visit(r);

      //Generate all Block-Instantiations. Only for complex Blocks that need
      //multiple clock cycles, it is necessary to instantiate extra Blocks.

      //TODO disabled on redesign ob block infrastructure
#if 0
      for (base_p B : CurrentKernel->getBlocks()) {

        //find Vhdl Component instances for HW-Blocks
        auto It = HWMap.find( B.get() );

        if (It != HWMap.end() ) {
          auto VhdlComp = It->second;
          CurrentKernelEntity->addComponent( VhdlComp );

          //Iterate through all In and Out ports and instantiate signals if they
          //are not Kernel outputs

          //if ( ! B->Out.empty() ) {
          //  auto It = std::find(r.Out.begin(), r.Out.end(), O );
          //  if ( It != r.Out.end() ) {
          //    KernelEntity->addSignal(std::make_shared<VhdlSignal>(O->getUniqueName(), O->getBitwidth() ));
          //  }
          //}
        } else {
          /* */
        }
      }
#endif

      visitAll();

      CurrentKernelEntity->write();
      return 0;
    }

#if 0
    virtual int visit(InStreamIndex &r)
    {
      VISIT_ONCE(r);
      DEBUG_CALL;
      //super::visit(r);
      return 0;
    }
#endif

    virtual int visit(ScalarPort &r)
    {
      VISIT_ONCE(r);
      DEBUG_CALL;

      super::visit(r);
      return 0;
    }

    virtual int visit(StreamPort &r)
    {
      VISIT_ONCE(r);
      DEBUG_CALL;

      bool In = false;

      for (streamindex_p I : r.getIndices()) {
        std::shared_ptr<VhdlMemPort> Mem;
        if (In)
          Mem = std::make_shared<VhdlMemInPort>(I->getUniqueName(), r.getBitwidth());
        else
          Mem = std::make_shared<VhdlMemOutPort>(I->getUniqueName(), r.getBitwidth());

        CurrentKernelEntity->InMem.push_back(Mem);
        SignalMap[I.get()] = Mem->getData();
        //errs() << "Add " << I->getUniqueName() << "\n" << " to Signal List\n";

        if (In && ( I->getIns().size() != 0 || I->getOuts().size() == 0)) {
          llvm_unreachable("InStream In/Out failure.");
        } else if (!In && (I->getIns().size() == 0 || I->getOuts().size() != 0)) {
          llvm_unreachable("OutStream In/Out failure.");
        }

#if 0
        if (I->getIns().size() != 1) {
          errs() << "StreamIndex In: " << I->getIns().size() << " " << I->dump("\t") << "\n";
          for (base_p P : I->getIns()) {
            errs() << P->dump("\t\t") << "\n";
          }
          llvm_unreachable("Multiple Inputs of Stream-Index");
        }
#endif

        if (dynamicstreamindex_p DI = std::dynamic_pointer_cast<DynamicStreamIndex>(I)) {
          signal_p Index;
          base_p HWIndex = DI->getIndex();

          auto IndexIt = SignalMap.find(HWIndex.get());
          if (IndexIt != SignalMap.end())
            Index = IndexIt->second;
          else {
            Index = std::make_shared<VhdlSignal>(HWIndex->getUniqueName(), HWIndex->getBitwidth());
            SignalMap[HWIndex.get()] = Index;
            errs() << "Creating new Index Signal " << Index->getUniqueName() << "\n";
          }

          CurrentKernelEntity->addSignalAssignment(Mem->getAddr(), Index);

        } else if (staticstreamindex_p SI = std::dynamic_pointer_cast<StaticStreamIndex>(I)) {
          CurrentKernelEntity->addValueAssignment(Mem->getAddr(), SI->getIndex());
        }
      }
      super::visit(r);
      return 0;
    }

#if 0
    virtual int visit(InStream &r)
    {
      VISIT_ONCE(r);
      DEBUG_CALL;
      //pass
      super::visit(r);
      return 0;
    }

    virtual int visit(OutStream &r)
    {
      VISIT_ONCE(r);
      DEBUG_CALL;
      //pass
      super::visit(r);
      return 0;
    }
#endif

    virtual int visit(StreamIndex &r)
    {
      VISIT_ONCE(r);
      DEBUG_CALL;
      super::visit(r);
      return 0;
    }

    virtual int visit(DynamicStreamIndex &r)
    {
      VISIT_ONCE(r);
      DEBUG_CALL;

      super::visit(r);
      return 0;
    }

    virtual int visit(StaticStreamIndex &r)
    {
      VISIT_ONCE(r);
      DEBUG_CALL;

      super::visit(r);
      return 0;
    }

    virtual int visit(Arith &r)
    {
      VISIT_ONCE(r);
      DEBUG_CALL;

      std::stringstream buf;
      buf<< r.getUniqueName() << r.getUID();

      if (r.getIns().size() != 2)
        llvm_unreachable("Implement Arith for multiple inputs");
      if (r.getOuts().size() == 0)
        llvm_unreachable("No output");

      //find Input and Output signals
      base_p HWOps[3] = {r.getOut(0), r.getIn(0), r.getIn(1)};
      signal_p Ops[3];

      auto It = SignalMap.find(&r);
      if ( It == SignalMap.end() ) {
          Ops[0] = std::make_shared<VhdlSignal>( r.getUniqueName(), r.getBitwidth());
          SignalMap[&r] = Ops[0];
      } else Ops[0] = It->second;

      for (int i = 1; i < 3; ++i) {
        auto It = SignalMap.find(HWOps[i].get());
        if ( It == SignalMap.end() ) {
          if (const_p C = std::dynamic_pointer_cast<ConstVal>(HWOps[i])) {
            Ops[i] = std::make_shared<VhdlSignal>( HWOps[i]->getName(), HWOps[i]->getBitwidth() );
            errs() << "Creating new Constant Signal " << HWOps[i]->getName() << "\n";
          } else {
            //Ops[i] = std::make_shared<VhdlSignal>( HWOps[i]->getUniqueName(), HWOps[i]->getBitwidth() );
            errs() << "Not all Input signals available: " << HWOps[i]->getUniqueName() << "\n" << r.dump("\t") << "\n";
            VISIT_RESET(r);
            return VISIT_AGAIN;
          }

          SignalMap[ HWOps[i].get() ] = Ops[i];
        } else Ops[i] = It->second;
      }

      // differentiate
      std::shared_ptr<VhdlCombinational> C;
      if (dynamic_cast<Add *>(&r)) {
        C = std::make_shared<VhdlIntAdd>( Ops[0], Ops[1], Ops[2]  );
        CurrentKernelEntity->addCombinational(C);
      } else if (dynamic_cast<FAdd *>(&r)) {
        C = std::make_shared<VhdlIntAdd>( Ops[0], Ops[1], Ops[2]  );
        CurrentKernelEntity->addCombinational(C);
      } else if (dynamic_cast<Sub  *>(&r)) {
        C = std::make_shared<VhdlIntAdd>( Ops[0], Ops[1], Ops[2]  );
        CurrentKernelEntity->addCombinational(C);
      } else if (dynamic_cast<FSub *>(&r)) {
        C = std::make_shared<VhdlIntAdd>( Ops[0], Ops[1], Ops[2]  );
        CurrentKernelEntity->addCombinational(C);
      } else if (dynamic_cast<Mul  *>(&r)) {
        C = std::make_shared<VhdlIntMul>( Ops[0], Ops[1], Ops[2]  );
        CurrentKernelEntity->addCombinational(C);
      } else if (dynamic_cast<FMul *>(&r)) {
        C = std::make_shared<VhdlIntAdd>( Ops[0], Ops[1], Ops[2]  );
        CurrentKernelEntity->addCombinational(C);
      } else if (dynamic_cast<UDiv *>(&r)) {
        C = std::make_shared<VhdlIntAdd>( Ops[0], Ops[1], Ops[2]  );
        CurrentKernelEntity->addCombinational(C);
      } else if (dynamic_cast<SDiv *>(&r)) {
        C = std::make_shared<VhdlIntAdd>( Ops[0], Ops[1], Ops[2]  );
        CurrentKernelEntity->addCombinational(C);
      } else if (dynamic_cast<FDiv *>(&r)) {
        C = std::make_shared<VhdlIntAdd>( Ops[0], Ops[1], Ops[2]  );
        CurrentKernelEntity->addCombinational(C);
      } else if (dynamic_cast<URem *>(&r)) {
        C = std::make_shared<VhdlIntAdd>( Ops[0], Ops[1], Ops[2]  );
        CurrentKernelEntity->addCombinational(C);
      } else if (dynamic_cast<SRem *>(&r)) {
        C = std::make_shared<VhdlIntAdd>( Ops[0], Ops[1], Ops[2]  );
        CurrentKernelEntity->addCombinational(C);
      } else if (dynamic_cast<FRem *>(&r)) {
        C = std::make_shared<VhdlIntAdd>( Ops[0], Ops[1], Ops[2]  );
        CurrentKernelEntity->addCombinational(C);
      } else if (dynamic_cast<And  *>(&r)) {
        C = std::make_shared<VhdlIntAdd>( Ops[0], Ops[1], Ops[2]  );
        CurrentKernelEntity->addCombinational(C);
      } else if (dynamic_cast<Or   *>(&r)) {
        C = std::make_shared<VhdlIntAdd>( Ops[0], Ops[1], Ops[2]  );
        CurrentKernelEntity->addCombinational(C);
      }


      super::visit(r);
      return 0;
    }

#if 0
      auto It = SignalMap.find(HWOp0);
      if ( It == SignalMap.end() ) {
        Op0 = std::make_shared<VhdlSignal>( HWOp0->getUniqueName(), HWOp0->getBitwidth() );
        SignalMap[ HWOp0 ] = Op0;
      } else Op0 = It->second;

      It = SignalMap.find( HWOp1 );
      if ( It == SignalMap.end() ) {
        if (ConstVal C = std::dynamic_pointer_cast<ConstVal>(r.In.
        Op1 = std::make_shared<VhdlSignal>(HWOp1->getUniqueName(), HWOpt1->getBitwidth());
        SignalMap[ HWOp1 ] = Op1;
      } else Op1 = It->second;

      // Only needed to allocate a single Signal, even for multiple outputs.

      It = SignalMap.find( HWT );
      if ( It == SignalMap.end() ) {
        T = std::make_shared<VhdlSignal>(HWT->getUniqueName(), HWT->getBitwidth());
        SignalMap[ HWT ] = T;
      } else T = It->second;
#endif
      /*


      for (auto &I : r.In ) {
        DEBUG( llvm::dbgs() << "Input: " << TYPENAME(*I) << " with name " << I->getUniqueName() <<  "\n" );
        auto *C = dynamic_cast<ConstVal*>(I.get());
        if ( C  ) {
          KAdd->In.push_back( ;
        } else {
          KAdd->In.push_back( std::make_shared<VhdlInPort>( I->getUniqueName(), I->getBitwidth() ) );
        }
      }

      for (auto &O : r.Out ) {
        auto *std::make_shared<VhdlOutPort>(r.getUniqueName(), I->getBitwidth() ) );
      }

         auto KAdd = std::make_shared<VhdlIntAdd>( );


      HWMap[&r] = ; 
      */

 //   virtual int visit(Sub &r)
 //   {
 //     VISIT_ONCE(r);
 //     DEBUG_CALL;
 //     
 //     super::visit(r);
 //     return 0;
 //   }

    virtual int visit(Compare &r)
    {
      VISIT_ONCE(r);
      DEBUG_CALL;
      super::visit(r);
      return 0;
    }

    virtual int visit(Mux &r)
    {
      VISIT_ONCE(r);
      DEBUG_CALL;
      super::visit(r);
      return 0;
    }

    virtual int visit(Reg &r)
    {
      VISIT_ONCE(r);
      DEBUG_CALL;
      super::visit(r);
      return 0;
    }

    virtual int visit(Ram &r)
    {
      VISIT_ONCE(r);
      DEBUG_CALL;
      super::visit(r);
      return 0;
    }

    virtual int visit(Fifo &r)
    {
      VISIT_ONCE(r);
      DEBUG_CALL;
      super::visit(r);
      return 0;
    }

    virtual int visit(ConstVal &r)
    {
      VISIT_ONCE(r);
      DEBUG_CALL;
      super::visit(r);
      return 0;
    }
};
} //end namespace vhdl
} //end namespace oclacc

#undef TYPENAME
#undef DEBUG_CALL

#endif /* VHDL_KERNEL_H */
