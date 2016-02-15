include ../../../../Makefile.include

CXX_SRC = $(wildcard *.cpp) 
CXX_DEP = $(patsubst %.cpp,%.d,$(CXX_SRC))
CXX_OBJ = $(patsubst %.cpp,%.o,$(CXX_SRC))
CXX_BIN = $(patsubst %.cpp,%.bin,$(CXX_SRC))


.PHONY: dot run

DOT_SRC := $(wildcard *.dot)
DOT_PNG := $(patsubst %.dot,%.png,$(DOT_SRC))
DOT_SVG := $(patsubst %.dot,%.svg,$(DOT_SRC))

CXXFLAGS+=-I../../ 
LDFLAGS+=-L../ -lhw

all: $(CXX_BIN)

%.d: %.cpp
	$(CXX) -MM $(CXXFLAGS) $< -MF $@
	sed -i 's/$*\.o[:]*/$*\.o $@:/g' $@

-include $(CXX_DEP)

%.bin: %.o
	$(CXX) $(CXXFLAGS) $< $(LDFLAGS) -o $@

png: $(DOT_PNG)

%.svg: %.dot
	dot -Tsvg -o$@ $<
%.png: %.dot
	dot -Tpng -o$@ $<
%.png: %.dot
	dot -Tpng -o$@ $<

clean:
	rm -f test $(CXX_DEP) $(CXX_OBJ) $(CXX_BIN)

distclean:
	rm -f $(DOT_PNG) $(DOT_SRC) $(DOT_SVG)
