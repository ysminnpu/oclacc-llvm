set(LLVM_TARGET_DEFINITIONS OCLAcc.td)
tablegen(LLVM OCLAccGenSubtargetInfo.inc -gen-subtarget)
add_public_tablegen_target(OCLAccCommonTableGen)

add_llvm_target(OCLAccCodeGen
  OCLAccHWPass.cpp
  OCLAccSPIRCheckVisitor.cpp
  OCLAccTargetMachine.cpp
  OCLAccSubtarget.cpp
  CreateBlocksPass.cpp
  GenerateVhdlPass.cpp
  GenerateVerilogPass.cpp
  )

add_subdirectory(TargetInfo)
add_subdirectory(Backend)
add_subdirectory(HW)
