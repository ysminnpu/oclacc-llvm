add_llvm_library(LLVMOCLAccPasses
  Loopus.cpp
  HDLLoopUnroll.cpp
  SimplifyIDCalls.cpp
  HDLPromoteID.cpp
  OpenCLMDKernels.cpp
  HDLInliner.cpp
  HDLFlattenCFG.cpp
  HardwareModel.cpp
  SplitBarrierBlocks.cpp
  DelayStores.cpp
  BitWidthAnalysis.cpp
  ShiftRegisterDetection.cpp
  LoopusUtils.cpp
  RewriteExpr.cpp
  AggregateLoads.cpp
  InstSimplify.cpp
  ArgPromotionTracker.cpp
  CanonicalizePredecessors.cpp
  CFGOptimizer.cpp
  FindAllPaths.cpp
  RenameInvalid.cpp
  PrintModule.cpp
)

add_dependencies(LLVMOCLAccPasses intrinsics_gen)
