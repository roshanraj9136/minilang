source "/home/roshan-raj/minilang/emsdk/emsdk_env.sh"

emcc src/lexer.cpp src/ast.cpp src/parser.cpp src/semantic.cpp src/codegen.cpp src/opcode.cpp src/vm.cpp src/disassembler.cpp src/wasm_api.cpp \
  -o web/minilang.js \
  -s EXPORTED_RUNTIME_METHODS='["ccall", "cwrap"]' \
  -s EXPORTED_FUNCTIONS='["_wasm_compile", "_wasm_run", "_wasm_disasm", "_wasm_debug_start", "_wasm_debug_step", "_wasm_debug_current_line", "_wasm_debug_current_ip", "_wasm_debug_stack", "_wasm_debug_variables", "_wasm_debug_output", "_wasm_debug_bytecode"]' \
  -std=c++17 -O3 -s NO_EXIT_RUNTIME=1 -s ALLOW_MEMORY_GROWTH=0 -fexceptions
