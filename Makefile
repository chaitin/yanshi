ifeq ($(build),release)
  BUILD := release
  CFLAGS += -g3 -Os -std=c++1y
else
  BUILD := build
  CFLAGS += -g3 -std=c++1y -fsanitize=undefined,address -DDEBUG
  LDLIBS := -lasan -lubsan
endif

CPPFLAGS := -Isrc -I.
CXXFLAGS := $(CFLAGS)
LDLIBS := -licuuc
SRC := $(filter-out src/lexer.cc src/parser.cc, $(wildcard src/*.cc)) src/lexer.cc src/parser.cc
OBJ := $(addprefix $(BUILD)/,$(subst src/,,$(SRC:.cc=.o)))
UNITTEST_SRC := $(wildcard unittest/*.cc)
UNITTEST_EXE := $(subst unittest/,,$(UNITTEST_SRC:.cc=))

all: $(BUILD)/yanshi # unittest

unittest: $(addprefix $(BUILD)/unittest/,$(UNITTEST_EXE))
	$(foreach x,$(addprefix $(BUILD)/unittest/,$(UNITTEST_EXE)),$x && ) :

sinclude $(OBJ:.o=.d)

$(BUILD) $(BUILD)/unittest:
	mkdir -p $@

$(BUILD)/yanshi: $(OBJ)
	$(LINK.cc) $^ $(LDLIBS) -o $@

$(BUILD)/%.o: src/%.cc | $(BUILD)
	g++ $(CPPFLAGS) -MM -MP -MT $@ -MF $(@:.o=.d) $<
	$(COMPILE.cc) $< -o $@

$(BUILD)/unittest/%: unittest/%.cc $(wildcard unittest/*.hh) $(filter-out $(BUILD)/main.o,$(OBJ)) | $(BUILD)/unittest
	g++ $(CPPFLAGS) -MM -MP -MT $@ -MF $(@:.o=.d) $<
	$(LINK.cc) $(filter-out %.hh,$^) $(LDLIBS) -o $@

src/lexer.cc src/lexer.hh: src/lexer.l
	flex --header-file=src/lexer.hh -o src/lexer.cc $<

src/parser.cc src/parser.hh: src/parser.y src/common.hh src/location.hh src/option.hh src/syntax.hh
	bison --defines=src/parser.hh -o src/parser.cc $<

$(BUILD)/loader.o: src/parser.hh
$(BUILD)/parser.o: src/lexer.hh
$(BUILD)/lexer.o: src/parser.hh

clean:
	$(RM) -r build release

distclean: clean
	$(RM) src/{lexer,parser}.{cc,hh}

.PHONY: all clean distclean
