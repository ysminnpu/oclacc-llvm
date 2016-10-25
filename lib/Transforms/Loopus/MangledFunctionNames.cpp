//===- MangledFunctionNames.cpp -------------------------------------------===//
//===----------------------------------------------------------------------===//

#include "MangledFunctionNames.h"

Loopus::MangledFunctionNames Loopus::MangledFunctionNames::instance;

/// \brief Creates a new instance but does not nothing.
///
/// Creates a new name manager. The constructor does nothing. All
/// initializations should be done in the \c initialize function which is just
/// called when an instance is really needed.
Loopus::MangledFunctionNames::MangledFunctionNames(void)
 : isInitialized(false) {
}

/// \brief Initializes the instance.
// TODO: Might be that the mangled name of get_local_linear_id and
// get_global_linear_id is wrong
void Loopus::MangledFunctionNames::initialize(void) {
  // Add functions of Workitem category
  add("_Z12get_work_dim"              , "get_work_dim"            , 0,
      BuiltInFunctionName::FunctionCategory::FC_Workitem);
  add("_Z15get_global_sizej"          , "get_global_size"         , 1,
      BuiltInFunctionName::FunctionCategory::FC_Workitem);
  add("_Z13get_global_idj"            , "get_global_id"           , 1,
      BuiltInFunctionName::FunctionCategory::FC_Workitem);
  add("_Z14get_local_sizej"           , "get_local_size"          , 1,
      BuiltInFunctionName::FunctionCategory::FC_Workitem);
  add("_Z23get_enqueued_local_sizej"  , "get_enqueued_local_size" , 1,
      BuiltInFunctionName::FunctionCategory::FC_Workitem);
  add("_Z12get_local_idj"             , "get_local_id"            , 1,
      BuiltInFunctionName::FunctionCategory::FC_Workitem);
  add("_Z14get_num_groupsj"           , "get_num_groups"          , 1,
      BuiltInFunctionName::FunctionCategory::FC_Workitem);
  add("_Z12get_group_idj"             , "get_group_id"            , 1,
      BuiltInFunctionName::FunctionCategory::FC_Workitem);
  add("_Z17get_global_offsetj"        , "get_global_offset"       , 1,
      BuiltInFunctionName::FunctionCategory::FC_Workitem);
  add("_Z20get_global_linear_idv"     , "get_global_linear_id"    , 0,
      BuiltInFunctionName::FunctionCategory::FC_Workitem);
  add("_Z19get_local_linear_idv"      , "get_local_linear_id"     , 0,
      BuiltInFunctionName::FunctionCategory::FC_Workitem);
  // Add functions of synchronization category
  add("_Z18work_group_barrierj"       , "work_group_barrier"      , 1,
      BuiltInFunctionName::FunctionCategory::FC_Sync);
  add("_Z18work_group_barrierjj"      , "work_group_barrier"      , 2,
      BuiltInFunctionName::FunctionCategory::FC_Sync);
  add("_Z7barrierj"                   , "barrier"                 , 1,
      BuiltInFunctionName::FunctionCategory::FC_Sync);
  // Add functions of printf category
  add("_Z6printfPrU3AS2cz"            , "printf"                  , 1,
      BuiltInFunctionName::FunctionCategory::FC_Printf);

  // Add functions of math category
  add("_Z4acosf"                      , "acos"                    , 1,
      BuiltInFunctionName::FunctionCategory::FC_Math);
  add("_Z4acosDv2_f"                  , "acos"                    , 1,
      BuiltInFunctionName::FunctionCategory::FC_Math);
  add("_Z4acosDv3_f"                  , "acos"                    , 1,
      BuiltInFunctionName::FunctionCategory::FC_Math);
  add("_Z4acosDv4_f"                  , "acos"                    , 1,
      BuiltInFunctionName::FunctionCategory::FC_Math);
  add("_Z4acosDv8_f"                  , "acos"                    , 1,
      BuiltInFunctionName::FunctionCategory::FC_Math);
  add("_Z4acosDv16_f"                 , "acos"                    , 1,
      BuiltInFunctionName::FunctionCategory::FC_Math);
  add("_Z4acosd"                      , "acos"                    , 1,
      BuiltInFunctionName::FunctionCategory::FC_Math);
  add("_Z4acosDv2_d"                  , "acos"                    , 1,
      BuiltInFunctionName::FunctionCategory::FC_Math);
  add("_Z4acosDv3_d"                  , "acos"                    , 1,
      BuiltInFunctionName::FunctionCategory::FC_Math);
  add("_Z4acosDv4_d"                  , "acos"                    , 1,
      BuiltInFunctionName::FunctionCategory::FC_Math);
  add("_Z4acosDv8_d"                  , "acos"                    , 1,
      BuiltInFunctionName::FunctionCategory::FC_Math);
  add("_Z4acosDv16_d"                 , "acos"                   , 1,
      BuiltInFunctionName::FunctionCategory::FC_Math);

  add("_Z4asinf"                      , "asin"                    , 1,
      BuiltInFunctionName::FunctionCategory::FC_Math);
  add("_Z4asinDv2_f"                  , "asin"                    , 1,
      BuiltInFunctionName::FunctionCategory::FC_Math);
  add("_Z4asinDv3_f"                  , "asin"                    , 1,
      BuiltInFunctionName::FunctionCategory::FC_Math);
  add("_Z4asinDv4_f"                  , "asin"                    , 1,
      BuiltInFunctionName::FunctionCategory::FC_Math);
  add("_Z4asinDv8_f"                  , "asin"                    , 1,
      BuiltInFunctionName::FunctionCategory::FC_Math);
  add("_Z4asinDv16_f"                 , "asin"                    , 1,
      BuiltInFunctionName::FunctionCategory::FC_Math);
  add("_Z4asind"                      , "acos"                    , 1,
      BuiltInFunctionName::FunctionCategory::FC_Math);
  add("_Z4asinDv2_d"                  , "acos"                    , 1,
      BuiltInFunctionName::FunctionCategory::FC_Math);
  add("_Z4asinDv3_d"                  , "acos"                    , 1,
      BuiltInFunctionName::FunctionCategory::FC_Math);
  add("_Z4asinDv4_d"                  , "acos"                    , 1,
      BuiltInFunctionName::FunctionCategory::FC_Math);
  add("_Z4asinDv8_d"                  , "acos"                    , 1,
      BuiltInFunctionName::FunctionCategory::FC_Math);
  add("_Z4asinDv16_d"                 , "acos"                    , 1,
      BuiltInFunctionName::FunctionCategory::FC_Math);

  add("_Z3cosf"                       , "cos"                     , 1,
      BuiltInFunctionName::FunctionCategory::FC_Math);
  add("_Z3cosDv2_f"                   , "cos"                     , 1,
      BuiltInFunctionName::FunctionCategory::FC_Math);
  add("_Z3cosDv3_f"                   , "cos"                     , 1,
      BuiltInFunctionName::FunctionCategory::FC_Math);
  add("_Z3cosDv4_f"                   , "cos"                     , 1,
      BuiltInFunctionName::FunctionCategory::FC_Math);
  add("_Z3cosDv8_f"                   , "cos"                     , 1,
      BuiltInFunctionName::FunctionCategory::FC_Math);
  add("_Z3cosDv16_f"                  , "cos"                     , 1,
      BuiltInFunctionName::FunctionCategory::FC_Math);
  add("_Z3cosd"                       , "cos"                     , 1,
      BuiltInFunctionName::FunctionCategory::FC_Math);
  add("_Z3cosDv2_d"                   , "cos"                     , 1,
      BuiltInFunctionName::FunctionCategory::FC_Math);
  add("_Z3cosDv3_d"                   , "cos"                     , 1,
      BuiltInFunctionName::FunctionCategory::FC_Math);
  add("_Z3cosDv4_d"                   , "cos"                     , 1,
      BuiltInFunctionName::FunctionCategory::FC_Math);
  add("_Z3cosDv8_d"                   , "cos"                     , 1,
      BuiltInFunctionName::FunctionCategory::FC_Math);
  add("_Z3cosDv16_d"                  , "cos"                     , 1,
      BuiltInFunctionName::FunctionCategory::FC_Math);

  add("_Z3sinf"                       , "sin"                     , 1,
      BuiltInFunctionName::FunctionCategory::FC_Math);
  add("_Z3sinDv2_f"                   , "sin"                     , 1,
      BuiltInFunctionName::FunctionCategory::FC_Math);
  add("_Z3sinDv3_f"                   , "sin"                     , 1,
      BuiltInFunctionName::FunctionCategory::FC_Math);
  add("_Z3sinDv4_f"                   , "sin"                     , 1,
      BuiltInFunctionName::FunctionCategory::FC_Math);
  add("_Z3sinDv8_f"                   , "sin"                     , 1,
      BuiltInFunctionName::FunctionCategory::FC_Math);
  add("_Z3sinDv16_f"                  , "sin"                     , 1,
      BuiltInFunctionName::FunctionCategory::FC_Math);
  add("_Z3sind"                       , "sin"                     , 1,
      BuiltInFunctionName::FunctionCategory::FC_Math);
  add("_Z3sinDv2_d"                   , "sin"                     , 1,
      BuiltInFunctionName::FunctionCategory::FC_Math);
  add("_Z3sinDv3_d"                   , "sin"                     , 1,
      BuiltInFunctionName::FunctionCategory::FC_Math);
  add("_Z3sinDv4_d"                   , "sin"                     , 1,
      BuiltInFunctionName::FunctionCategory::FC_Math);
  add("_Z3sinDv8_d"                   , "sin"                     , 1,
      BuiltInFunctionName::FunctionCategory::FC_Math);
  add("_Z3sinDv16_d"                  , "sin"                     , 1,
      BuiltInFunctionName::FunctionCategory::FC_Math);

  add("llvm.fmuladd.f32"              , "fmuladd.f32"             , 3,
      BuiltInFunctionName::FunctionCategory::FC_IntrinsicMath);
  add("llvm.fmuladd.f64"              , "fmuladd.f64"             , 3,
      BuiltInFunctionName::FunctionCategory::FC_IntrinsicMath);

  isInitialized = true;
}

/// \brief Adds a function name to the list of available names.
/// \param MangledName The mangled function name.
/// \param UnmangledName The unmangled function name.
/// \param Params The number of parameters the function expects.
/// \param FunctionCat The category of the function.
void Loopus::MangledFunctionNames::add(const std::string &MangledName,
    const std::string &UnmangledName, unsigned Params,
    Loopus::BuiltInFunctionName::FunctionCategory FunctionCat) {
  if ((MangledName.size() == 0) || (MangledName.length() == 0)) {
    return;
  }
  if ((UnmangledName.size() == 0) || (UnmangledName.length() == 0)) {
    return;
  }

  BuiltInFunctionName bifn;
  bifn.MangledName = MangledName;
  bifn.UnmangledName = UnmangledName;
  bifn.NumParams = Params;
  bifn.FuncCat = FunctionCat;

  AvailableNames[bifn.MangledName] = bifn;
}

/// \brief Returns an instance of the name manager.
///
/// Initializes and returns an instance of the name manager.
const Loopus::MangledFunctionNames& Loopus::MangledFunctionNames::getInstance(void) {
  if (instance.isInitialized == false) {
    instance.initialize();
  }
  return instance;
}

/// \brief Determines if the (mangled) function name is known.
/// \param name The name to look up.
bool Loopus::MangledFunctionNames::isKnownName(const std::string &name) const {
  const auto result = AvailableNames.find(name);
  if (result == AvailableNames.end()) {
    return false;
  } else {
    return true;
  }
}

bool Loopus::MangledFunctionNames::isKnownUnmangledName(const std::string &name) const {
  for (auto NIT = AvailableNames.cbegin(), NEND = AvailableNames.cend();
      NIT != NEND; ++NIT) {
    if (NIT->second.UnmangledName.compare(name) == true) {
      return true;
    }
  }
  return false;
}

/// \brief Determines if the given name exists and if its category is WorkItem.
/// \param name The name of the function to test.
bool Loopus::MangledFunctionNames::isWorkItemFunction(const std::string &name) const {
  const auto result = AvailableNames.find(name);
  if (result == AvailableNames.end()) {
    return false;
  }
  if (result->second.FuncCat == BuiltInFunctionName::FunctionCategory::FC_Workitem) {
    return true;
  } else {
    return false;
  }
}

/// \brief Determines if the given name exists and if its category is Printf.
/// \param name The name of the function to test.
bool Loopus::MangledFunctionNames::isPrintfFunction(const std::string &name) const {
  const auto result = AvailableNames.find(name);
  if (result == AvailableNames.end()) {
    return false;
  }
  if (result->second.FuncCat == BuiltInFunctionName::FunctionCategory::FC_Printf) {
    return true;
  } else {
    return false;
  }
}

/// \brief Determines if the given name exists and if its category is Synchronization.
/// \param name The name of the function to test.
bool Loopus::MangledFunctionNames::isSynchronizationFunction(const std::string &name) const {
  const auto result = AvailableNames.find(name);
  if (result == AvailableNames.end()) {
    return false;
  }
  if (result->second.FuncCat == BuiltInFunctionName::FunctionCategory::FC_Sync) {
    return true;
  } else {
    return false;
  }
}

/// \brief Returns the unmangled name of the given mangled name if it exists.
std::string Loopus::MangledFunctionNames::unmangleName(const std::string &name) const {
  const auto result = AvailableNames.find(name);
  if (result == AvailableNames.end()) {
    return name;
  }
  return result->second.UnmangledName;
}

/// \brief Returns the mangled name of the given unmangled name if it exists.
std::string Loopus::MangledFunctionNames::mangleName(const std::string &name) const {
  for (auto NIT = AvailableNames.cbegin(), NEND = AvailableNames.cend();
      NIT != NEND; ++NIT) {
    if (NIT->second.UnmangledName.compare(name) == 0) {
      return NIT->second.MangledName;
    }
  }
  return name;
}

