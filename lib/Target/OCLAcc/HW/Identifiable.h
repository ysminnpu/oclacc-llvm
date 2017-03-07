#ifndef IDENTIFIABLE_H
#define IDENTIFIABLE_H

#include <string>

#include "../Macros.h"

namespace oclacc {

class Identifiable
{
  public:
    typedef unsigned UIDTy;

  private:
    static unsigned currUID;

  protected:
    const UIDTy UID;
    std::string Name;

  public:

    Identifiable(const std::string &);

    virtual ~Identifiable() {};

    Identifiable (const Identifiable &) = delete;
    Identifiable &operator =(const Identifiable &) = delete;

    UIDTy getUID() const;
    virtual const std::string getName() const;
    virtual const std::string getUniqueName() const;

    void setName(const std::string &Name);
};

} //ns ocalcc

#endif /* IDENTIFIABLE_H */
