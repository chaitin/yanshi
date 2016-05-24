CXXFLAGS := -g3 -Isrc
O := $(addprefix src/,common.o lex.yy.o main.o parser.o)

all: yanshi

yanshi: $O
	$(LINK.cc) $^ -o $@

src/common.o: src/common.hh

src/lex.yy.o: src/parser.hh

src/lex.yy.cc: src/lexer.l
	flex -o $@ $<

src/parser.cc src/parser.hh: src/parser.y
	bison --defines=src/parser.hh -o src/parser.cc $<

clean:
	$(RM) src/lex.yy.cc src/parser.cc src/parser.hh src/*.o

.PHONY: all clean
