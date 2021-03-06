//===-- oclacc-llc.cpp - Implement the LLVM Native Code Generator ----------------===//


#include "llvm/ADT/Triple.h"
#include "llvm/CodeGen/CommandFlags.h"
#include "llvm/CodeGen/LinkAllAsmWriterComponents.h"
#include "llvm/CodeGen/LinkAllCodegenComponents.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/IR/IRPrintingPasses.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IRReader/IRReader.h"
#include "llvm/MC/SubtargetFeature.h"
#include "llvm/Pass.h"
#include "llvm/PassManager.h"
#include "llvm/Analysis/CFGPrinter.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/FormattedStream.h"
#include "llvm/Support/Host.h"
#include "llvm/Support/ManagedStatic.h"
#include "llvm/Support/PluginLoader.h"
#include "llvm/Support/PrettyStackTrace.h"
#include "llvm/Support/Signals.h"
#include "llvm/Support/SourceMgr.h"
#include "llvm/Support/TargetRegistry.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Support/ToolOutputFile.h"
#include "llvm/Target/TargetLibraryInfo.h"
#include "llvm/Target/TargetMachine.h"
#include "llvm/Target/TargetSubtargetInfo.h"

#include <memory>
#include <dirent.h>
#include <unistd.h>
#include <sys/stat.h>

#include "Target.h"

// TODO
// find better way to include these passes. Eventually move them out of oclacc
//#include "../src/OCLAcc/OCLAccSPIRCheckVisitor.h"

using namespace llvm;

// General options for llc.  Other pass-specific options are specified
// within the corresponding llc passes, and target-specific options
// and back-end code generation options are specified with the target machine.
//
static cl::opt<std::string>
InputFilename(cl::Positional, cl::desc("<input bitcode>"), cl::init("-"));

static cl::opt<std::string>
OutputFilename("o", cl::desc("Output filename"), cl::value_desc("filename"));

static cl::opt<unsigned>
TimeCompilations("time-compilations", cl::Hidden, cl::init(1u),
                 cl::value_desc("N"),
                 cl::desc("Repeat compilation N times for timing"));

static cl::opt<bool>
NoIntegratedAssembler("no-integrated-as", cl::Hidden,
                      cl::desc("Disable integrated assembler"));

// Determine optimization level.
static cl::opt<char>
OptLevel("O",
         cl::desc("Optimization level. [-O0, -O1, -O2, or -O3] "
                  "(default = '-O2')"),
         cl::Prefix,
         cl::ZeroOrMore,
         cl::init(' '));

static cl::opt<std::string>
TargetTriple("mtriple", cl::desc("Override target triple for module"));

static cl::opt<bool> NoVerify("disable-verify", cl::Hidden,
                              cl::desc("Do not verify input module"));

static cl::opt<bool> DisableSimplifyLibCalls("disable-simplify-libcalls",
                                             cl::desc("Disable simplify-libcalls"));

static cl::opt<bool> ShowMCEncoding("show-mc-encoding", cl::Hidden,
                                    cl::desc("Show encoding in .s output"));

static cl::opt<bool> EnableDwarfDirectory(
    "enable-dwarf-directory", cl::Hidden,
    cl::desc("Use .file directives with an explicit directory."));

static cl::opt<bool> AsmVerbose("asm-verbose",
                                cl::desc("Add comments to directives."),
                                cl::init(true));
/*
 * OCLAcc Options
 */

static cl::opt<std::string> OutputDir("oclacc-dir", cl::desc("Output directory. Module Name if not set.") );
static cl::opt<std::string> ModuleName("oclacc-module", cl::desc("Module Name. Must be set if using stdin, otherwise optional.") );

static int compileModule(char **, LLVMContext &);

/* 
 * handle the target platform argument
 */
struct DeviceParser : public cl::parser<oclacc::TargetDevice> {
  bool parse( cl::Option &, StringRef, const std::string &, oclacc::TargetDevice &);
};

/* Val is target, Arg is supplied argument that has to be checked */
bool DeviceParser::parse( cl::Option &O, StringRef ArgName, 
    const std::string &ArgValue, oclacc::TargetDevice &Val ) {

  bool valid = !(ArgValue.compare("s5phq_d5"));

  if (valid) Val = oclacc::BITTWARE_S5PHQ_D5;

  if (!valid)  O.error("Invalid TargetDevice: " + ArgValue);

  return !valid;
}

// FIXME: external storage argument or not
static cl::opt<oclacc::TargetDevice, false, DeviceParser> 
  TargetDevice("target", cl::init(oclacc::BITTWARE_S5PHQ_D5), cl::desc("Target Platform (Default: Bittware s5phq-d5).") );

// Output file type handling not necessary
#if 0
static std::unique_ptr<tool_output_file>
GetOutputStream(const char *TargetName, Triple::OSType OS,
                const char *ProgName) {
  // If we don't yet have an output filename, make one.
  if (OutputFilename.empty()) {
    if (InputFilename == "-")
      OutputFilename = "-";
    else {
      // If InputFilename ends in .bc or .ll, remove it.
      StringRef IFN = InputFilename;
      if (IFN.endswith(".bc") || IFN.endswith(".ll"))
        OutputFilename = IFN.drop_back(3);
      else
        OutputFilename = IFN;

      switch (FileType) {
      case TargetMachine::CGFT_AssemblyFile:
        if (TargetName[0] == 'c') {
          if (TargetName[1] == 0)
            OutputFilename += ".cbe.c";
          else if (TargetName[1] == 'p' && TargetName[2] == 'p')
            OutputFilename += ".cpp";
          else
            OutputFilename += ".s";
        } else
          OutputFilename += ".s";
        break;
      case TargetMachine::CGFT_ObjectFile:
        if (OS == Triple::Win32)
          OutputFilename += ".obj";
        else
          OutputFilename += ".o";
        break;
      case TargetMachine::CGFT_Null:
        OutputFilename += ".null";
        break;
      }
    }
  }

  // Decide if we need "binary" output.
  bool Binary = false;
  switch (FileType) {
  case TargetMachine::CGFT_AssemblyFile:
    break;
  case TargetMachine::CGFT_ObjectFile:
  case TargetMachine::CGFT_Null:
    Binary = true;
    break;
  }
  bool Binary = false;

  // Open the file.
  std::error_code EC;
  sys::fs::OpenFlags OpenFlags = sys::fs::F_None;
  if (!Binary)
    OpenFlags |= sys::fs::F_Text;
  auto FDOut = llvm::make_unique<tool_output_file>(OutputFilename, EC,
                                                   OpenFlags);
  if (EC) {
    errs() << EC.message() << '\n';
    return nullptr;
  }

  return FDOut;
}
#endif

// main - Entry point for the llc compiler.
//
int main(int argc, char **argv) {
  sys::PrintStackTraceOnErrorSignal();
  PrettyStackTraceProgram X(argc, argv);

  // Enable debug stream buffering.
  EnableDebugBuffering = true;

  LLVMContext &Context = getGlobalContext();
  llvm_shutdown_obj Y;  // Call llvm_shutdown() on exit.

  // Initialize targets first, so that --version shows registered targets.
  InitializeAllTargets();
  InitializeAllTargetMCs();
  InitializeAllAsmPrinters();
  InitializeAllAsmParsers();

  // Initialize codegen and IR passes used by llc so that the -print-after,
  // -print-before, and -stop-after options work.
  PassRegistry *Registry = PassRegistry::getPassRegistry();
  initializeCore(*Registry);
  initializeCodeGen(*Registry);
  initializeLoopStrengthReducePass(*Registry);
  initializeLowerIntrinsicsPass(*Registry);
  initializeUnreachableBlockElimPass(*Registry);

  // Register the target printer for --version.
  cl::AddExtraVersionPrinter(TargetRegistry::printRegisteredTargetsForVersion);

  cl::ParseCommandLineOptions(argc, argv, "llvm system compiler\n");

  // Compile the module TimeCompilations times to give better compile time
  // metrics.
  for (unsigned I = TimeCompilations; I; --I)
    if (int RetVal = compileModule(argv, Context))
      return RetVal;
  return 0;
}

static int compileModule(char **argv, LLVMContext &Context) {
  // Load the module to be compiled...
  SMDiagnostic Err;
  std::unique_ptr<Module> M;
  Triple TheTriple;

  bool SkipModule = MCPU == "help" ||
                    (!MAttrs.empty() && MAttrs.front() == "help");

  // If user asked for the 'native' CPU, autodetect here. If autodection fails,
  // this will set the CPU to an empty string which tells the target to
  // pick a basic default.
  if (MCPU == "native")
    MCPU = sys::getHostCPUName();

  // If user just wants to list available options, skip module loading
  if (!SkipModule) {
    M = parseIRFile(InputFilename, Err, Context);
    if (!M) {
      Err.print(argv[0], errs());
      return 1;
    }

    // If we are supposed to override the target triple, do so now.
    if (!TargetTriple.empty())
      M->setTargetTriple(Triple::normalize(TargetTriple));
    TheTriple = Triple(M->getTargetTriple());
  } else {
    TheTriple = Triple(Triple::normalize(TargetTriple));
  }

  if (TheTriple.getTriple().empty())
    TheTriple.setTriple(sys::getDefaultTargetTriple());

  // Get the target specific parser.
  std::string Error;
  const Target *TheTarget = TargetRegistry::lookupTarget(MArch, TheTriple,
                                                         Error);
  if (!TheTarget) {
    errs() << argv[0] << ": " << Error;
    return 1;
  }

  // Package up features to be passed to target/subtarget
  std::string FeaturesStr;
  if (MAttrs.size()) {
    SubtargetFeatures Features;
    for (unsigned i = 0; i != MAttrs.size(); ++i)
      Features.AddFeature(MAttrs[i]);
    FeaturesStr = Features.getString();
  }

  CodeGenOpt::Level OLvl = CodeGenOpt::Default;
  switch (OptLevel) {
  default:
    errs() << argv[0] << ": invalid optimization level.\n";
    return 1;
  case ' ': break;
  case '0': OLvl = CodeGenOpt::None; break;
  case '1': OLvl = CodeGenOpt::Less; break;
  case '2': OLvl = CodeGenOpt::Default; break;
  case '3': OLvl = CodeGenOpt::Aggressive; break;
  }

  TargetOptions Options = InitTargetOptionsFromCodeGenFlags();
  Options.DisableIntegratedAS = NoIntegratedAssembler;
  Options.MCOptions.ShowMCEncoding = ShowMCEncoding;
  Options.MCOptions.MCUseDwarfDirectory = EnableDwarfDirectory;
  Options.MCOptions.AsmVerbose = AsmVerbose;

  std::unique_ptr<TargetMachine> Target(
      TheTarget->createTargetMachine(TheTriple.getTriple(), MCPU, FeaturesStr,
                                     Options, RelocModel, CMModel, OLvl));
  assert(Target && "Could not allocate target machine!");

  // If we don't have a module then just exit now. We do this down
  // here since the CPU/Feature help is underneath the target machine
  // creation.
  if (SkipModule)
    return 0;

  assert(M && "Should have exited if we didn't have a module!");

  if (GenerateSoftFloatCalls)
    FloatABIForCalls = FloatABI::Soft;

  //OCLAcc change directory for output according to Module
  StringRef MN = M->getName();

  if (!MN.compare("<stdin>")) {
    if (!ModuleName.compare("")) {
      errs() << "No valid module name. Use --oclacc-module for stdin\n";
      return -1;
    } else 
      MN = ModuleName;
  }

  if (MN.endswith(".bc") || MN.endswith(".ll"))
    MN = MN.drop_back(3);

  // remove all path parts
  size_t LastSlash = MN.find_last_of('/');
  if (LastSlash != MN.npos) {
    MN = MN.drop_front(LastSlash+1);
  }

  if (!OutputDir.compare("")) 
    OutputDir = MN;


  std::string WorkDir = OutputDir;
  std::string Module = MN;

  M->setModuleIdentifier(Module);

  errs() << "ModuleName: " << Module << "\n";
  errs() << "WorkDir: " << WorkDir << "\n";

  std::error_code EC;

  EC = sys::fs::create_directories(WorkDir);
  if (EC) {
    errs() << "Failed to open logfile: " << EC.message() << '\n';
    return -1;
  }

  SmallVector<char, 1024> CD;
  EC = sys::fs::current_path(CD);
  if (EC) {
    errs() << "Failed to get current dir: " << EC.message() << '\n';
    return -1;
  }

  std::string CurrentDir(CD.data(), CD.size());

  if (chdir(WorkDir.c_str())) {
    errs() << "Failed to change to " << CurrentDir.data() << ": " << strerror(errno) << "\n";
    return -1;
  }

  //OCLAcc

  // Open the file.

  //auto Out = llvm::make_unique<tool_output_file>(MN.str()+".log", EC, sys::fs::F_Text);
  auto Log = std::make_unique<raw_fd_ostream>(Module+".log", EC, llvm::sys::fs::F_RW | llvm::sys::fs::F_Text);
  if (EC) {
    errs() << "Failed to open logfile: " << EC.message() << '\n';
    return -1;
  }
  auto FLog = std::make_unique<formatted_raw_ostream>(*Log);

  // FIXME: Declare success. 
  // Might be done only at the end but for now it is
  // better to keep output.
  //Out->keep();

  // Build up all of the passes that we want to do to the module.
  PassManager PM;

  // Add an appropriate TargetLibraryInfo pass for the module's triple.
  TargetLibraryInfo *TLI = new TargetLibraryInfo(TheTriple);
  if (DisableSimplifyLibCalls)
    TLI->disableAllFunctions();
  PM.add(TLI);

  // Add the target data from the target machine, if it exists, or the module.
  if (const DataLayout *DL = Target->getSubtargetImpl()->getDataLayout())
    M->setDataLayout(DL);
  PM.add(new DataLayoutPass());

  if (RelaxAll.getNumOccurrences() > 0 &&
      FileType != TargetMachine::CGFT_ObjectFile)
    errs() << argv[0]
             << ": warning: ignoring -mc-relax-all because filetype != obj";

  {
    AnalysisID StartAfterID = nullptr;
    AnalysisID StopAfterID = nullptr;
    const PassRegistry *PR = PassRegistry::getPassRegistry();
    if (!StartAfter.empty()) {
      const PassInfo *PI = PR->getPassInfo(StartAfter);
      if (!PI) {
        errs() << argv[0] << ": start-after pass is not registered.\n";
        return 1;
      }
      StartAfterID = PI->getTypeInfo();
    }
    if (!StopAfter.empty()) {
      const PassInfo *PI = PR->getPassInfo(StopAfter);
      if (!PI) {
        errs() << argv[0] << ": stop-after pass is not registered.\n";
        return 1;
      }
      StopAfterID = PI->getTypeInfo();
    }


    // Ask the target to add backend passes as necessary.
    if (Target->addPassesToEmitFile(PM, *FLog, FileType, NoVerify,
          StartAfterID, StopAfterID)) {
      errs() << argv[0] << ": target does not support generation of this"
        << " file type!\n";
      return 1;
    }

    // Before executing passes, print the final values of the LLVM options.
    cl::PrintOptionValues();


    PM.run(*M);

  }


  if (chdir(CurrentDir.data())) {
    errs() << "Failed to change back to " << CurrentDir.data() << ": " << strerror(errno) << "\n";
    return -1;
  }

  return 0;
}
