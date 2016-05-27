CXXFLAGS := -g3 -Isrc -std=c++1y -fsanitize=undefined,address
O := $(addprefix src/,common.o lexer_helper.o lexer.o loader.o location.o main.o option.o parser.o syntax.o)

fsa: src/fsa.cc
	$(LINK.cc) $^ -o $@

all: yanshi

yanshi: $O
	$(LINK.cc) $^ -o $@

src/parser.o: src/lexer.hh
src/lexer.o: src/parser.hh

src/main.o: src/parser.hh src/syntax.hh

src/lexer.cc src/lexer.hh: src/lexer.l
	flex --header-file=src/lexer.hh -o src/lexer.cc $<

src/parser.cc src/parser.hh: src/parser.y src/location.hh src/syntax.hh
	bison --defines=src/parser.hh -o src/parser.cc $<

clean:
	$(RM) src/*.o

distclean: clean
	$(RM) src/{lexer,parser}.{cc,hh}

.PHONY: all clean distclean
