#ifndef WRITEABLE_H
#define WRITEABLE_H

#include "llvm/Support/FileSystem.h"
#include "llvm/Support/raw_ostream.h"
#include "Macros.h"

namespace oclacc {

class Writeable {
  protected:
    std::string FileName;
    std::unique_ptr<llvm::raw_fd_ostream> F;

    Writeable () {
    }

    Writeable ( const std::string &FileName ) : FileName(FileName) {
    }

    void openFile() {
      if (FileName.empty())
        llvm_unreachable("Filename empty");

      DEBUG_WITH_TYPE("FileIO", llvm::dbgs() << "Open File "+ FileName << "\n" );

      std::error_code Err;
      F = std::unique_ptr<llvm::raw_fd_ostream>( new llvm::raw_fd_ostream( FileName, Err, llvm::sys::fs::F_RW | llvm::sys::fs::F_Text ));

      if (Err)
        llvm_unreachable("File could not be opened.");

    }

    void closeFile() {
      DEBUG_WITH_TYPE("FileIO", llvm::dbgs() << "Close File "+ FileName << "\n" );
      F->close();
    }

  public:
    virtual ~Writeable( ) {};

    NO_COPY_ASSIGN(Writeable)

    virtual void write(const std::string &Indent) = 0;
};

} //end namespace oclacc

#endif /* WRITEABLE_H */
