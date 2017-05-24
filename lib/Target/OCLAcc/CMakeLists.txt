set(LLVM_TARGET_DEFINITIONS OCLAcc.td)
tablegen(LLVM OCLAccGenSubtargetInfo.inc -gen-subtarget)
add_public_tablegen_target(OCLAccCommonTableGen)

add_llvm_target(OCLAccCodeGen
  OCLAccSPIRCheckVisitor.cpp
  OCLAccTargetMachine.cpp
  OCLAccSubtarget.cpp
  OCLAccHW.cpp
  GEP.cpp
  Utils.cpp
  OCL/NameMangling.cpp
  )

add_subdirectory(Passes)
add_subdirectory(TargetInfo)
add_subdirectory(Backend)
add_subdirectory(HW)
add_subdirectory(OCL)
