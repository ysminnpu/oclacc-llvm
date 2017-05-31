#ifndef VERILOGMODULE_H
#define VERILOGMODULE_H

#include <list>

namespace oclacc {

class Component;

/// \brief Base class to implement Components
class VerilogModule {
  public:
    typedef std::list<std::string> FileListTy;

  private:
    Component &Comp;

    FileListTy Files;

  public:
    VerilogModule(Component &);
    virtual ~VerilogModule();

    virtual const std::string declHeader() const = 0;

    const std::string declFooter() const;

    virtual void genTestBench() const;

    inline void addFile(const std::string F) {
      Files.push_back(F);
    }

    inline FileListTy &getFiles() {
      return Files;
    }

    inline const FileListTy &getFiles() const {
      return Files;
    }
};

/// \brief Instantiate multiple Kernel Functions in one Design
class TopModule {
};

} // end ns oclacc

#endif /* VERILOGMODULE_H */
