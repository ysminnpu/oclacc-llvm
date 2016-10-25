//===- MangledFunctionNames.h ---------------------------------------------===//
//
// This file provides a class that stores the (mangled) names of the OpenCL
// builtin functions.
//
//===----------------------------------------------------------------------===//

#ifndef _LOOPUS_MANGLEDFUNCTIONNAMES_H_INCLUDE_
#define _LOOPUS_MANGLEDFUNCTIONNAMES_H_INCLUDE_

#include <map>
#include <string>

namespace Loopus {

struct BuiltInFunctionName {
  enum FunctionCategory {
    FC_Others         = 0     ,
    FC_Workitem       = 1 << 0,
    FC_Printf         = 1 << 1,
    FC_Sync           = 1 << 2,
    FC_Math           = 1 << 3,
    FC_IntrinsicMath  = 1 << 4
  };

  BuiltInFunctionName(void)
   : MangledName(""), UnmangledName(""), FuncCat(FunctionCategory::FC_Others),
     NumParams(0) {
  }

  std::string MangledName;
  std::string UnmangledName;
  FunctionCategory FuncCat;
  unsigned NumParams;
};

class MangledFunctionNames {
private:
  MangledFunctionNames(void);
  MangledFunctionNames(const MangledFunctionNames &copy) = delete;
  MangledFunctionNames& operator=(const MangledFunctionNames &copy) = delete;

  void initialize(void);
  void add(const std::string &MangledName, const std::string &UnmangledName,
      unsigned Params, BuiltInFunctionName::FunctionCategory FunctionCat);

  static MangledFunctionNames instance;

  bool isInitialized;
  std::map<std::string, BuiltInFunctionName> AvailableNames;

public:
  static const MangledFunctionNames& getInstance(void);

  bool isKnownName(const std::string &name) const;
  bool isKnownUnmangledName(const std::string &name) const;
  bool isWorkItemFunction(const std::string &name) const;
  bool isPrintfFunction(const std::string &name) const;
  bool isSynchronizationFunction(const std::string &name) const;
  std::string unmangleName(const std::string &name) const;
  std::string mangleName(const std::string &name) const;
};

} // End of Loopus namespace

#endif

