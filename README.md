# MiniLang — A C++17 Compiler, Virtual Machine, and Web Playground

MiniLang is a complete, self-contained compiler pipeline, virtual machine interpreter, and interactive web playground built from scratch in modern C++17. It compiles standard C++ syntax (including templates, pointers, and streams) into a custom stack-based bytecode format, executes it on a virtual CPU, compiles it natively using a transpiler backend, and features a terminal TUI debugger alongside a WebAssembly-powered browser playground.

---

## Compiler Architecture Pipeline

```
  Source Code (.ml)
         │
         ▼
 ┌───────────────┐
 │     Lexer     │  (Character-by-character scan -> Tokens & Namespace stripping)
 └───────────────┘
         │
         ▼
 ┌───────────────┐
 │    Parser     │  (Recursive Descent & Grouped Operator Climbing -> AST)
 └───────────────┘
         │
         ▼
 ┌───────────────┐
 │   Semantics   │  (Symbol Table & Type Checker -> Annotated AST)
 └───────────────┘
         │
  ┌──────┴──────────────┐
  ▼                     ▼
┌───────────────┐     ┌───────────────┐
│   Codegen     │     │  Transpiler   │
└───────────────┘     └───────────────┘
  (Bytecode VM)         (Native C++17)
```

---

## Key Features

* **Standard C++ Streams**: Natively supports `cout <<` and `cin >>` statements with chained inputs/outputs and `endl` translation.
* **C++ Templates & Collections**: Supports standard library template declarations, constructor initializations, and member methods:
  * `vector<T>`: `push_back(val)`, `pop_back()`, `size()`, `empty()`, `clear()`, and nested matrix vectors (e.g. `vector<vector<int>>`).
  * `pair<T1, T2>`: `.first` and `.second` member access.
  * `queue<T>`: `push(val)`, `pop()`, `front()`, `size()`, `empty()`.
  * `stack<T>`: `push(val)`, `pop()`, `top()`, `size()`, `empty()`.
* **Value-Copy Semantics**: Vector constructors deep-copy default structures (such as nested vectors inside matrices) to match C++ constructor value allocation semantics.
* **Pointers & References**: Declares pointer types (`int*`), dereferences pointers (`*ptr`), and references memory addresses (`&x`).
* **Control Flow**: Supports `if` / `else if` / `else`, conditional `while` loops, standard `for` loops, `do-while` loops, and `switch-case` statements with `break`/`continue` controls.
* **Compile-Time Queries**: Supports `sizeof` type size calculations and `typeof` runtime type inspection.

---

## Getting Started

### 1. Build the Compiler
Building requires a C++17 compliant compiler (`g++` or `clang++`) and `make`.

```bash
make
```

### 2. Run the Test Suite
The compiler includes a test suite covering all arithmetic, loops, templates, pointers, and C++ stream features.

```bash
bash ./tests/run_tests.sh
```

---

## How to Use

### 1. Run a Program in the Custom VM
Executes a MiniLang source file on the custom virtual machine.
```bash
./minilang run tests/programs/16_templates.ml
```

### 2. Compile Natively
Transpiles the MiniLang AST into a standard C++ program and compiles it with your local `g++` compiler into a native binary.
```bash
./minilang compile tests/programs/17_matrix.ml -o matrix_calc
./matrix_calc
```

### 3. Open the Interactive TUI Debugger
Launches a visual Terminal User Interface to step through VM bytecodes, inspect the operand stack, and track local scopes in real-time.
```bash
./minilang debug tests/programs/18_pointer_swap.ml
```
* **Step Instruction**: Press `Enter` or type `s`
* **Step Over Line**: Type `n` or `next`
* **Continue Execution**: Type `c` or `continue`
* **Toggle Breakpoint**: Type `b <line_number>`

---

## Interactive Web Playground

The compiler is compiled to WebAssembly (using Emscripten) and runs locally in a browser interface, complete with a Monaco code editor and a web-based debugger terminal simulator.

### 1. Build WebAssembly Binaries
```bash
bash build_wasm.sh
```

### 2. Start the Local Server
```bash
python3 -m http.server 8000 --directory web
```
Open **[http://localhost:8000](http://localhost:8000)** in your browser to view the playground, load high-level showcases (2D DP pathfinding, Graph BFS traversal, matrix multiplication, pointer swaps), and run them live!
