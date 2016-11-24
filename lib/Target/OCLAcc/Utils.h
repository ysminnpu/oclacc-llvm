#ifndef UTILS_H
#define UTILS_H

#include <string>
#include <memory>

namespace llvm {
class raw_fd_ostream;
}

typedef std::shared_ptr<llvm::raw_fd_ostream> FileTy;

FileTy openFile(const std::string &);

void changeDir(const std::string &);

#endif /* UTILS_H */
