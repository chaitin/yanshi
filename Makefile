CPPFLAGS := -Isrc -I. -DDEBUG
CFLAGS := -g3 -std=c++1y -fsanitize=undefined,address
CXXFLAGS := $(CFLAGS)
LDLIBS := $(CFLAGS) -lasan -lubsan
SRC := $(filter-out src/lexer.cc src/parser.cc, $(wildcard src/*.cc)) src/lexer.cc src/parser.cc
OBJ := $(addprefix build/,$(subst src/,,$(SRC:.cc=.o)))
UNITTEST_SRC := $(wildcard unittest/*.cc)
UNITTEST_EXE := $(subst unittest/,,$(UNITTEST_SRC:.cc=))

all: build/yanshi # unittest

unittest: $(addprefix build/unittest/,$(UNITTEST_EXE))
	$(foreach x,$(addprefix build/unittest/,$(UNITTEST_EXE)),$x && ) :

sinclude $(OBJ:.o=.d)

build build/unittest:
	mkdir -p $@

build/yanshi: $(OBJ)
	$(LINK.cc) $^ $(LDLIBS) -o $@

build/%.o: src/%.cc | build
	g++ $(CPPFLAGS) -MM -MP -MT $@ -MF $(@:.o=.d) $<
	$(COMPILE.cc) $< -o $@

build/unittest/%: unittest/%.cc $(wildcard unittest/*.hh) $(filter-out build/main.o,$(OBJ)) | build/unittest
	g++ $(CPPFLAGS) -MM -MP -MT $@ -MF $(@:.o=.d) $<
	$(LINK.cc) $(filter-out %.hh,$^) $(LDLIBS) -o $@

src/lexer.cc src/lexer.hh: src/lexer.l
	flex --header-file=src/lexer.hh -o src/lexer.cc $<

src/parser.cc src/parser.hh: src/parser.y src/location.hh src/syntax.hh
	bison --defines=src/parser.hh -o src/parser.cc $<

build/loader.o: src/parser.hh
build/parser.o: src/lexer.hh
build/lexer.o: src/parser.hh

clean:
	$(RM) -r build

distclean: clean
	$(RM) src/{lexer,parser}.{cc,hh}

.PHONY: all clean distclean
