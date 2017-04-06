#ifndef VERILOGMACROS_H
#define VERILOGMACROS_H

#define Indent(C) std::string((C)*2,' ')
#define BEGIN(S) {S << Indent(++II) << "begin\n";}while(0);
#define END(S) {S << Indent(II--) << "end\n";}while(0);

#endif /* VERILOGMACROS_H */
