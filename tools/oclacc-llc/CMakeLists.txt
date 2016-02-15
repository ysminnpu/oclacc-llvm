set(LLVM_LINK_COMPONENTS
  ${LLVM_TARGETS_TO_BUILD}
  AsmPrinter
  CodeGen
  Core
  IRReader
  MC
  ScalarOpts
  SelectionDAG
  Support
  Target
  )

# Support plugins.
set(LLVM_NO_DEAD_STRIP 1)

add_llvm_tool(oclacc-llc
  oclacc-llc.cpp
  )
set_target_properties(oclacc-llc PROPERTIES ENABLE_EXPORTS 1)
