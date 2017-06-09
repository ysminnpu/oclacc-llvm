# OCLAcc
Open Source Configurable Logic Block based Accelerators

## Build OCLAcc
OCLAcc itself is implemented as LLVM TargetMachine, so LLVM has to be built.
Refer to the [llmv documentation](http://llvm.org/docs/GettingStarted.html) for details.

An easy way to build it is by using `cmake`. You can use the following script to create a separate build directory:
```bash
#!/bin/bash
set -u

export CC=clang-3.8
export CXX=clang++-3.8
set -u
PWD=`pwd`
mkdir -p build
mkdir -p root

cd build
cmake -G "Unix Makefiles" --with-gcc-toolchain  -DCMAKE_BUILD_TYPE=Debug \
-DCMAKE_INSTALL_PREFIX="$PWD/root" -DLLVM_REQUIRES_RTTI=on \
-DLLVM_ENABLE_DOXYGEN=on \
-DLLVM_ENABLE_CXX1Y=on \
-DLLVM_ENABLE_ASSERTIONS=on \
-DLLVM_TARGETS_TO_BUILD="OCLAcc" $OCLACC_SRC
cd $PWD
```
Set `$OCLACC_SRC` to point to the local copy of this repository.

It is sufficient to build `oclacc-llc` by running `make oclacc-llc` in the build dir.

## Own kernels

Is is necessary to create SPIR input from OpenCL kernel files for OCLAcc.

OCLAcc currently works with SPIR 2.0 generated by Khronos SPIR compiler:

`https://github.com/KhronosGroup/SPIR/tree/spir_20_provisional`

Use branch `spir_20_provisional`. Build and install it.

## Run test kernels

You can use the pre-compiled kernels from [oclacc-kernels](https://github.com/sifrrich/oclacc-kernels).

## Generate dot graph
`oclacc-llc -march=dot <kernel>.bc`

## Generate verilog
`oclacc-llc -march=dot <kernel>.bc`

## Debug output
`oclacc-llc -march=dot <kernel>.bc -debug`
