#ifndef DESIGNFILES_H
#define DESIGNFILES_H

#include <list>
#include <string>

namespace oclacc {

class DesignFiles {
  public:
    typedef std::list<std::string> FileListTy;
    typedef std::list<std::string>::const_iterator FileListConstItTy;

  private:
    FileListTy Files;
    // Arithmetic Operators to be generated with Flopoco

  public:
    inline void addFile(const std::string S) {
      Files.push_back(S);
    }

    void write(const std::string Filename);
};
} // end ns oclacc;

#endif /* DESIGNFILES_H */
