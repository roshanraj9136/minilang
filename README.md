# MiniLang Web Playground

Welcome to **MiniLang**, a custom statically-typed programming language, compiler, virtual machine, and visual debugger playground that I built from scratch! 

This playground allows you to write, run, and step through code directly in your browser.

**Live Demo**: [minilang-one.vercel.app](https://minilang-one.vercel.app/)

---

## What is MiniLang?
MiniLang is an imperative programming language designed to look and feel like standard C++. 

Instead of just compiling code to run invisibly, I wanted to create a tool that helps developers **see exactly how code runs on a CPU**. I built:
1. **A Custom Compiler**: Converts standard syntax (variables, functions, templates, and pointers) into custom stack-based bytecode instructions.
2. **A Stack-Based Virtual Machine (VM)**: An emulator that executes the compiled bytecode instructions.
3. **A WebAssembly Port**: The compiler and VM are compiled using Emscripten to run natively in your web browser.
4. **An Interactive IDE & Visual Debugger**: A dashboard featuring a code editor (Monaco, same engine that powers VS Code) and an instruction-level watch panel.

---

## How it Works (Under the Hood)
When you write code and click **Run** or **Debug**, the application executes a complete pipeline:

```
[ Your Code (.ml) ] -> [ Custom Compiler ] -> [ Stack Bytecode ] -> [ Stack VM (Wasm) ] -> [ Web Dashboard ]
```

* **Lexing & Parsing**: The editor scans your characters, validates syntax, and builds an Abstract Syntax Tree (AST).
* **Semantic Analysis**: Checks that variable types are correct (e.g. you aren't adding a string to an integer) and resolves template structures.
* **Bytecode Generation**: Translates the code into simple, low-level instructions (like `PUSH_INT`, `LOAD_LOCAL`, `ADD`).
* **Virtual CPU Execution**: The virtual machine steps through the bytecode, managing local variable scopes and an operand stack in memory.

---

## Playground Features
* **Modern Code Editor**: Syntax highlighting, auto-completions, and a clean dark theme.
* **Instruction-Level Watch Panels**:
  * **Source Code View**: Highlights the currently active line of code in blue as you execute.
  * **Bytecode Viewer**: Highlights the exact VM instruction being processed in green.
  * **Memory Watch**: Real-time display of active local variables in your current scope.
  * **Operand Stack**: Watch values get pushed and popped off the VM stack in real-time.
* **Interactive Code Showcases**: Ready-made showcases like a 2D Pathfinding Solver (Dynamic Programming), Graph BFS Traversal, Matrix Multiplication, and Pointer Swaps.

---

## MiniLang Code Syntax Cheat Sheet

Here are simple examples of the syntax supported in my language:

### 1. Variables and Math
```cpp
int main() {
    int a = 10;
    int b = 20;
    int sum = a + b;
    cout << "Sum is: " << sum << endl;
    return 0;
}
```

### 2. Standard C++ Collections (Vectors, Queues, Stacks)
```cpp
#include <iostream>
#include <vector>
using namespace std;

int main() {
    vector<int> numbers;
    numbers.push_back(5);
    numbers.push_back(10);
    
    cout << "Size of vector: " << numbers.size() << endl;
    return 0;
}
```

### 3. Pointers and Address References
```cpp
void swap(int* p1, int* p2) {
    int temp = *p1;
    *p1 = *p2;
    *p2 = temp;
}

int main() {
    int x = 5;
    int y = 10;
    swap(&x, &y);
    cout << "x is now: " << x << endl; // Prints 10
    return 0;
}
```

---

## Running the Playground Locally

If you want to clone my repository and run the website locally:

1. **Clone the repository**:
   ```bash
   git clone https://github.com/roshanraj9136/minilang.git
   cd minilang
   ```
2. **Start a local server**:
   You can use python to launch a simple HTTP server:
   ```bash
   python3 -m http.server 8000
   ```
3. **Open in browser**:
   Navigate to [http://localhost:8000/web](http://localhost:8000/web) in your browser.
