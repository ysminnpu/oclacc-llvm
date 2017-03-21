#ifndef UTILS_H
#define UTILS_H

#include <list>
#include <string>
#include <memory>

namespace llvm {
class raw_fd_ostream;
}

typedef std::shared_ptr<llvm::raw_fd_ostream> FileTy;

FileTy openFile(const std::string &);

void changeDir(const std::string &);

const std::string Line(79,'-');

#endif /* UTILS_H */
