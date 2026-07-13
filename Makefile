# MiniLang Makefile
# Native build: g++ with C++17, -O2, -g
# WASM build: emcc (Emscripten)

CXX = g++
CXXFLAGS = -Wall -Wextra -std=c++17 -O2 -g

# Native sources (excludes wasm_api.cpp)
NATIVE_SRC = src/main.cpp src/lexer.cpp src/ast.cpp src/parser.cpp \
             src/semantic.cpp src/codegen.cpp src/opcode.cpp src/vm.cpp \
             src/disassembler.cpp src/debugger.cpp src/transpiler.cpp
NATIVE_OBJ = $(NATIVE_SRC:.cpp=.o)
TARGET = minilang

# WASM sources (excludes main.cpp, includes wasm_api.cpp)
WASM_SRC = src/lexer.cpp src/ast.cpp src/parser.cpp src/semantic.cpp \
           src/codegen.cpp src/opcode.cpp src/vm.cpp src/disassembler.cpp \
           src/debugger.cpp src/transpiler.cpp src/wasm_api.cpp
WASM_TARGET_JS = web/minilang.js
WASM_TARGET_WASM = web/minilang.wasm

EMCC = emcc
EMCCFLAGS = -std=c++17 -O2 \
            -s WASM=1 \
            -s EXPORTED_RUNTIME_METHODS='["ccall","cwrap"]' \
            -s EXPORTED_FUNCTIONS='["_wasm_compile","_wasm_run","_wasm_debug_start","_wasm_debug_step","_wasm_debug_current_line","_wasm_debug_current_ip","_wasm_debug_stack","_wasm_debug_variables","_wasm_debug_output","_wasm_debug_bytecode","_wasm_disasm","_main"]' \
            -s ALLOW_MEMORY_GROWTH=0 \
            -s TOTAL_MEMORY=16777216 \
            --no-entry

# Default target
all: native

# Native build
native: $(TARGET)

$(TARGET): $(NATIVE_OBJ)
	$(CXX) $(CXXFLAGS) -o $@ $^

src/%.o: src/%.cpp
	$(CXX) $(CXXFLAGS) -c -o $@ $<

# WASM build
wasm: $(WASM_TARGET_JS)

$(WASM_TARGET_JS): $(WASM_SRC)
	@mkdir -p web
	$(EMCC) $(EMCCFLAGS) -o $@ $^

# Clean
clean:
	rm -f $(TARGET) src/*.o
	rm -f $(WASM_TARGET_JS) $(WASM_TARGET_WASM)

rebuild: clean all

.PHONY: all native wasm clean rebuild
