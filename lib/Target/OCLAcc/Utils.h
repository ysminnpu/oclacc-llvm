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

// taken from http://stackoverflow.com/a/2072890
inline bool ends_with(std::string const &value, std::string const &ending) {
    if (ending.size() > value.size()) return false;
    return std::equal(ending.rbegin(), ending.rend(), value.rbegin());
}

#endif /* UTILS_H */
