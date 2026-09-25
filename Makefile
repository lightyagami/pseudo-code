CXX ?= g++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra -pedantic -MMD -MP
TARGET = pseudoc

SRCS = $(wildcard src/*.cpp)
OBJS = $(SRCS:.cpp=.o)
DEPS = $(SRCS:.cpp=.d)

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^

-include $(DEPS)

src/%.o: src/%.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

PREFIX ?= /usr/local
BINDIR ?= $(PREFIX)/bin

test: $(TARGET)
	@bash tests/run_tests.sh

install: $(TARGET)
	install -d $(DESTDIR)$(BINDIR)
	install -m 755 $(TARGET) $(DESTDIR)$(BINDIR)/$(TARGET)

uninstall:
	rm -f $(DESTDIR)$(BINDIR)/$(TARGET)

WASM_TARGET = web/pseudoc.js
WASM_SRCS = src/wasm_api.cpp src/bytecode.cpp src/codegen.cpp src/py_codegen.cpp src/c_to_pseudo.cpp src/diagnostics.cpp src/lexer.cpp src/parser.cpp src/sema.cpp src/vm.cpp
EMCC ?= emcc
EMFLAGS ?= -std=c++17 -O2 -s WASM=1 \
    -s EXPORTED_RUNTIME_METHODS='["ccall","cwrap"]' \
    -s EXPORTED_FUNCTIONS='["_wasm_run_vm","_wasm_emit_c","_wasm_emit_py","_wasm_dump_bytecode","_wasm_check","_wasm_c_to_pseudo","_malloc","_free"]' \
    -s ALLOW_MEMORY_GROWTH=1 \
    -s MODULARIZE=1 \
    -s EXPORT_NAME='createPseudocModule'

wasm: $(WASM_TARGET)

$(WASM_TARGET): $(WASM_SRCS)
	@mkdir -p web
	$(EMCC) $(EMFLAGS) $(WASM_SRCS) -o $(WASM_TARGET)

clean:
	rm -f $(OBJS) $(DEPS) $(TARGET) sum.c sum test_sample.txt test_books.dat

.PHONY: all test install uninstall clean wasm
