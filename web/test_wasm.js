const Module = require('./minilang.js');
Module.onRuntimeInitialized = () => {
    console.log("Wasm loaded successfully!");
    try {
        const code = `fn fibonacci(n: int) -> int {
    if (n <= 1) {
        return n;
    }
    return fibonacci(n - 1) + fibonacci(n - 2);
}

fn main() {
    let i: int = 0;
    while (i < 10) {
        print(fibonacci(i));
        i = i + 1;
    }
}`;
        console.log("Compiling code...");
        const err = Module.ccall('wasm_compile', 'string', ['string'], [code]);
        console.log("Compile error output:", err);
        if (!err) {
            console.log("Running code...");
            const out = Module.ccall('wasm_run', 'string', [], []);
            console.log("Run stdout output:", out);
        }
    } catch (e) {
        console.error("Test failed with error:", e);
    }
    process.exit(0);
};
