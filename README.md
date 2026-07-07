# MiniLang Web Playground

MiniLang is an interactive web-based playground to write, run, and visually debug code in a custom statically-typed programming language. The code compiles to a custom stack-based bytecode and runs in the browser via a WebAssembly-powered virtual machine.

## Features
- **Monaco Code Editor**: Modern editor with syntax highlighting.
- **Visual Debugger**: Real-time instruction step-through, stack inspector, and local variable tracker.
- **Interactive Demos**: Try graph BFS, 2D grid pathfinding, matrix multiplication, and advanced pointer swaps.
- **Console Terminal**: Capture compile messages and virtual stdout/stderr output in real-time.

## Run Locally
To run the playground locally, launch a local HTTP server:
```bash
python3 -m http.server 8000
```
Then open [http://localhost:8000](http://localhost:8000) in your browser.
