set(LLVM_TARGET_DEFINITIONS OCLAcc.td)
tablegen(LLVM OCLAccGenSubtargetInfo.inc -gen-subtarget)
add_public_tablegen_target(OCLAccCommonTableGen)

add_llvm_target(OCLAccCodeGen
  CreateBlocksPass.cpp
  OCLAccSPIRCheckVisitor.cpp
  OCLAccTargetMachine.cpp
  OCLAccSubtarget.cpp
  OCLAccHWPass.cpp
  )

add_subdirectory(TargetInfo)
add_subdirectory(Backend)
add_subdirectory(HW)
