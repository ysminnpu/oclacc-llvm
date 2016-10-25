//===-- Loopus.h ------------------------------------------------*- C++ -*-===//
//
//===----------------------------------------------------------------------===//
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_TRANSFORMS_LOOPUS_H
#define LLVM_TRANSFORMS_LOOPUS_H

namespace llvm {
class BasicBlock;
class BasicBlockPass;
class Pass;

//===----------------------------------------------------------------------===//
//
// HDLLoopUnroll - Unroll loops to prepare them for HDL creation
//
Pass* createHDLLoopUnrollPass();

//===----------------------------------------------------------------------===//
//
// HDLPromoteID - Replace calls to OpenCL built-in functions
//
Pass* createHDLPromoteIDPass();

//===----------------------------------------------------------------------===//
//
// OpenCLMDKernels - Detect OpenCL kernel functions
//
Pass* createOpenCLMDKernelsPass();

//===----------------------------------------------------------------------===//
//
// HDLInliner - Inline functions into kernels
//
Pass* createHDLInlinerPass();

//===----------------------------------------------------------------------===//
//
// HDLFlattenCFG - Flatten CFG by using selects
//
Pass* createHDLFlattenCFGPass();

//===----------------------------------------------------------------------===//
//
// SplitBarrierBlocks - Split blocks using OpenCL barrier functions
//
Pass* createSplitBarrierBlocksPass();

//===----------------------------------------------------------------------===//
//
// DelayStores - Delay stores out of if-then-else
//
Pass* createDelayStoresPass();

//===----------------------------------------------------------------------===//
//
// BitWidthAnalysis - Analyze bit width of instructions' results
//
Pass* createBitWidthAnalysisPass();

//===----------------------------------------------------------------------===//
//
// ShiftRegisterDetection - Detect shift register loops
//
Pass* createShiftRegisterDetectionPass();

//===----------------------------------------------------------------------===//
//
// SimplifyIDCalls - Replace certain calls to ID-related built in functions
//
Pass* createSimplifyIDCallsPass();

//===----------------------------------------------------------------------===//
//
// RewriteExpr - Rewrite expressions to compute them in parallel
//
Pass* createRewriteExprPass();

//===----------------------------------------------------------------------===//
//
// AggregateLoads - Aggregate coalescing memory loads
//
Pass* createAggregateLoadsPass();

//===----------------------------------------------------------------------===//
//
// InstSimplify - Simplify instructions for hw generation
//
Pass* createInstSimplifyPass();

//===----------------------------------------------------------------------===//
//
// ArgPromotionTracker - Track promoted parameters
//
Pass* createArgPromotionTrackerPass();

//===----------------------------------------------------------------------===//
//
// CanoncalizePredecessors - Canonicalize pattern's predecessors
//
Pass* createCanonicalizePredecessorsPass();

//===----------------------------------------------------------------------===//
//
// CFGOptimizer - Optimize control flow graph
//
Pass* createCFGOptimizerPass();

} // End llvm namespace

#endif
