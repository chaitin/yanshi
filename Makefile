CPPFLAGS := -Isrc
CXXFLAGS := -g3 -std=c++1y -fsanitize=undefined,address
SRC := $(wildcard src/*.cc)
OBJ := $(addprefix build/,$(subst src/,,$(SRC:.cc=.o)))
UNITTEST_SRC := $(wildcard unittest/*.cc)
UNITTEST_EXE := $(subst unittest/,,$(UNITTEST_SRC:.cc=))

all: build/yanshi unittest

unittest: $(addprefix build/unittest/,$(UNITTEST_EXE))
	$(foreach x,$(addprefix build/unittest/,$(UNITTEST_EXE)),$x < in)

sinclude $(OBJ:.o=.d)

build build/unittest:
	mkdir -p $@

build/yanshi: $(OBJ)
	$(LINK.cc) $^ $(LDLIBS) -o $@

build/%.o: src/%.cc | build
	g++ $(CPPFLAGS) -MM -MP -MT $@ -MF $(@:.o=.d) $<
	$(COMPILE.cc) $< -o $@

build/unittest/%: unittest/%.cc $(filter-out build/main.o,$(OBJ)) | build/unittest
	g++ $(CPPFLAGS) -MM -MP -MT $@ -MF $(@:.o=.d) $<
	$(LINK.cc) $^ $(LDLIBS) -o $@

src/lexer.cc src/lexer.hh: src/lexer.l
	flex --header-file=src/lexer.hh -o src/lexer.cc $<

src/parser.cc src/parser.hh: src/parser.y src/location.hh src/syntax.hh
	bison --defines=src/parser.hh -o src/parser.cc $<

clean:
	$(RM) -r build

distclean: clean
	$(RM) src/{lexer,parser}.{cc,hh}

.PHONY: all clean distclean
