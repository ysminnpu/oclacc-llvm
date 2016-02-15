#ifndef MACROS_H
#define MACROS_H

#define NO_COPY_ASSIGN(TypeName) \
  TypeName(const TypeName&)=delete;      \
  TypeName &operator=(const TypeName &)=delete;

#endif /* MACROS_H */
