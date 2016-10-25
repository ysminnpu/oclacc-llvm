add_llvm_library(LLVMLoopus
	Loopus.cpp
	HDLLoopUnroll.cpp
  SimplifyIDCalls.cpp
	HDLPromoteID.cpp
	OpenCLMDKernels.cpp
	HDLInliner.cpp
	HDLFlattenCFG.cpp
	MangledFunctionNames.cpp
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
)

add_dependencies(LLVMLoopus intrinsics_gen)
