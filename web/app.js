const presets = {
    grid_dp_showcase: `#include <iostream>
#include <vector>
using namespace std;

int main() {
    int rows = 3;
    int cols = 3;
    vector<vector<int>> dp(rows, vector<int>(cols, 0));
    dp[0][0] = 1;
    for (int r = 0; r < rows; r++) {
        for (int c = 0; c < cols; c++) {
            if (r > 0) {
                dp[r][c] = dp[r][c] + dp[r - 1][c];
            }
            if (c > 0) {
                dp[r][c] = dp[r][c] + dp[r][c - 1];
            }
        }
    }
    cout << "Grid DP Paths Showcase:" << endl;
    for (int r = 0; r < rows; r++) {
        for (int c = 0; c < cols; c++) {
            cout << dp[r][c] << " ";
        }
        cout << endl;
    }
    return 0;
}`,
    graph_bfs_showcase: `#include <iostream>
#include <vector>
#include <queue>
using namespace std;

int main() {
    int num_nodes = 4;
    vector<vector<int>> adj(num_nodes, vector<int>());
    adj[0].push_back(1);
    adj[0].push_back(2);
    adj[1].push_back(3);
    adj[2].push_back(3);
    cout << "Graph BFS Traversal Showcase:" << endl;
    queue<int> q;
    vector<bool> visited(num_nodes, false);
    q.push(0);
    visited[0] = true;
    while (!q.empty()) {
        int curr = q.front();
        q.pop();
        cout << "Visited node: " << curr << endl;
        vector<int> neighbors = adj[curr];
        for (int i = 0; i < neighbors.size(); i++) {
            int next_node = neighbors[i];
            if (!visited[next_node]) {
                visited[next_node] = true;
                q.push(next_node);
            }
        }
    }
    return 0;
}`,
    matrix_showcase: `#include <iostream>
#include <vector>
using namespace std;

int main() {
    int n = 2;
    vector<vector<int>> A(n, vector<int>(n, 0));
    vector<vector<int>> B(n, vector<int>(n, 0));
    A[0][0] = 1; A[0][1] = 2;
    A[1][0] = 3; A[1][1] = 4;
    B[0][0] = 5; B[0][1] = 6;
    B[1][0] = 7; B[1][1] = 8;
    vector<vector<int>> C(n, vector<int>(n, 0));
    for (int i = 0; i < n; i++) {
        for (int j = 0; j < n; j++) {
            for (int k = 0; k < n; k++) {
                C[i][j] = C[i][j] + A[i][k] * B[k][j];
            }
        }
    }
    cout << "Matrix Multiplication Showcase:" << endl;
    for (int i = 0; i < n; i++) {
        for (int j = 0; j < n; j++) {
            cout << C[i][j] << " ";
        }
        cout << endl;
    }
    return 0;
}`,
    pointer_showcase: `#include <iostream>
using namespace std;

void swap(int* p1, int* p2) {
    int temp = *p1;
    *p1 = *p2;
    *p2 = temp;
}

int main() {
    int a = 10;
    int b = 20;
    int* ptrA = &a;
    int* ptrB = &b;
    cout << "Before swap: a = " << a << ", b = " << b << endl;
    swap(ptrA, ptrB);
    cout << "After swap: a = " << a << ", b = " << b << endl;
    return 0;
}`
};

let editor;
let isDebugging = false;
let isStepping = false;

require.config({ paths: { vs: 'https://cdnjs.cloudflare.com/ajax/libs/monaco-editor/0.39.0/min/vs' } });
require(['vs/editor/editor.main'], function () {
    monaco.editor.defineTheme('minilang-dark', {
        base: 'vs-dark',
        inherit: true,
        rules: [
            { token: 'keyword', foreground: '66fcf1', fontStyle: 'bold' },
            { token: 'comment', foreground: '45a29e', fontStyle: 'italic' },
            { token: 'string', foreground: 'e5c158' },
            { token: 'number', foreground: 'ff7b72' }
        ],
        colors: {
            'editor.background': '#1f2833',
            'editor.foreground': '#c5c6c7',
            'editorLineNumber.foreground': '#45a29e',
            'editorLineNumber.activeForeground': '#66fcf1'
        }
    });

    editor = monaco.editor.create(document.getElementById('editor'), {
        value: presets.grid_dp_showcase,
        language: 'rust',
        theme: 'minilang-dark',
        fontSize: 14,
        fontFamily: 'Fira Code',
        minimap: { enabled: false },
        automaticLayout: true
    });
});

document.getElementById('preset-selector').addEventListener('change', (e) => {
    if (editor) {
        editor.setValue(presets[e.target.value]);
    }
});

function logConsole(text, isError = false) {
    const consoleOutput = document.getElementById('console-output');
    if (isError) {
        const escaped = text.replace(/&/g, "&amp;").replace(/</g, "&lt;").replace(/>/g, "&gt;");
        consoleOutput.innerHTML = `<span style="color: #ff7b72; font-weight: 500;">${escaped}</span>`;
        return;
    }

    let formatted = text
        .replace(/&/g, "&amp;")
        .replace(/</g, "&lt;")
        .replace(/>/g, "&gt;")
        .replace(/\n/g, "<br>")
        .replace(/\[SUCCESS\]/g, '<span style="color: #66fcf1; font-weight: 600;">[SUCCESS]</span>')
        .replace(/\[COMPILE\]/g, '<span style="color: #66fcf1; font-weight: 600;">[COMPILE]</span>')
        .replace(/\[DEBUG\]/g, '<span style="color: #c5c6c7; font-weight: 600; font-style: italic;">[DEBUG]</span>')
        .replace(/\[INFO\]/g, '<span style="color: #45a29e; font-weight: 600;">[INFO]</span>')
        .replace(/\[SYSTEM\]/g, '<span style="color: #45a29e; font-weight: 600;">[SYSTEM]</span>');

    consoleOutput.innerHTML = formatted;
}

function updateTui() {
    const currentLine = Module.ccall('wasm_debug_current_line', 'number', [], []);
    const currentIp = Module.ccall('wasm_debug_current_ip', 'number', [], []);
    const stackContent = Module.ccall('wasm_debug_stack', 'string', [], []);
    const variablesContent = Module.ccall('wasm_debug_variables', 'string', [], []);
    const outputContent = Module.ccall('wasm_debug_output', 'string', [], []);
    const bytecodeContent = Module.ccall('wasm_debug_bytecode', 'string', [], []);

    document.getElementById('tui-ip-display').textContent = `IP: ${currentIp.toString().padStart(4, '0')}`;
    document.getElementById('tui-stack').textContent = stackContent;
    document.getElementById('tui-variables').textContent = variablesContent;
    document.getElementById('tui-bytecode').textContent = bytecodeContent;
    
    if (outputContent) {
        document.getElementById('console-output').textContent = outputContent;
    }

    const code = editor.getValue();
    const lines = code.split('\n');
    let sourceHtml = '';
    for (let i = 0; i < lines.length; ++i) {
        const lineNum = i + 1;
        const isActive = (lineNum === currentLine);
        const lineClass = isActive ? 'tui-line-active' : '';
        const mark = isActive ? '&gt;' : ' ';
        sourceHtml += `<div class="${lineClass}">${mark} ${lineNum.toString().padStart(2, ' ')} ${lines[i]}</div>`;
    }
    document.getElementById('tui-source').innerHTML = sourceHtml;
}

function enableDebugButtons(enabled) {
    document.getElementById('btn-step').disabled = !enabled;
    document.getElementById('btn-step-over').disabled = !enabled;
    document.getElementById('btn-continue').disabled = !enabled;
    document.getElementById('btn-stop').disabled = !enabled;
}

document.getElementById('btn-run').addEventListener('click', () => {
    try {
        if (isDebugging) stopDebugging();

        if (!editor) {
            logConsole("Error: Monaco editor is not initialized yet.", true);
            return;
        }

        const code = editor.getValue();
        logConsole("Compiling...");
        
        const compileErr = Module.ccall('wasm_compile', 'string', ['string'], [code]);
        if (compileErr) {
            logConsole(compileErr, true);
            return;
        }

        logConsole("Running...");
        switchTab('console');
        const output = Module.ccall('wasm_run', 'string', [], []);
        logConsole(output);
    } catch (err) {
        logConsole("JavaScript Error: " + err.message + "\n" + err.stack, true);
    }
});

document.getElementById('btn-debug').addEventListener('click', () => {
    try {
        if (isDebugging) {
            stopDebugging();
            return;
        }

        if (!editor) {
            logConsole("Error: Monaco editor is not initialized yet.", true);
            return;
        }

        const code = editor.getValue();
        logConsole("Compiling for debug...");

        const compileErr = Module.ccall('wasm_compile', 'string', ['string'], [code]);
        if (compileErr) {
            logConsole(compileErr, true);
            return;
        }

        const debugStarted = Module.ccall('wasm_debug_start', 'number', [], []);
        if (!debugStarted) {
            logConsole("Failed to start debug session", true);
            return;
        }

        isDebugging = true;
        switchTab('debugger');
        document.getElementById('btn-debug').textContent = "Stop Debugger";
        document.getElementById('btn-run').disabled = true;
        enableDebugButtons(true);
        updateTui();
        logConsole("[COMPILE] Success: Compiled target main\n[DEBUG] Debug session initialized\n[INFO] Call stack: main()\n[INFO] IP: 0000\n[INFO] Controls: Use Step, Next Line, or Continue.");
    } catch (err) {
        logConsole("JavaScript Debug Error: " + err.message + "\n" + err.stack, true);
    }
});

function stopDebugging() {
    isDebugging = false;
    isStepping = false;
    document.getElementById('btn-debug').textContent = "Debug Mode";
    document.getElementById('btn-run').disabled = false;
    enableDebugButtons(false);
    document.getElementById('tui-source').textContent = "";
    document.getElementById('tui-bytecode').textContent = "";
    document.getElementById('tui-stack').textContent = "";
    document.getElementById('tui-variables').textContent = "";
    logConsole("Debug session stopped.");
}

document.getElementById('btn-stop').addEventListener('click', stopDebugging);

document.getElementById('btn-step').addEventListener('click', () => {
    if (!isDebugging || isStepping) return;
    const running = Module.ccall('wasm_debug_step', 'number', [], []);
    updateTui();
    if (!running) {
        logConsole(Module.ccall('wasm_debug_output', 'string', [], []) + "\n\nProgram execution completed.");
        stopDebugging();
    }
});

document.getElementById('btn-step-over').addEventListener('click', () => {
    if (!isDebugging || isStepping) return;
    isStepping = true;
    enableDebugButtons(false);
    const startLine = Module.ccall('wasm_debug_current_line', 'number', [], []);

    function doStep() {
        if (!isDebugging) { isStepping = false; return; }
        const running = Module.ccall('wasm_debug_step', 'number', [], []);
        if (!running) {
            isStepping = false;
            updateTui();
            logConsole(Module.ccall('wasm_debug_output', 'string', [], []) + "\n\nProgram execution completed.");
            stopDebugging();
            return;
        }
        if (Module.ccall('wasm_debug_current_line', 'number', [], []) !== startLine) {
            isStepping = false;
            enableDebugButtons(true);
            updateTui();
            return;
        }
        setTimeout(doStep, 0);
    }
    doStep();
});

document.getElementById('btn-continue').addEventListener('click', () => {
    if (!isDebugging || isStepping) return;
    isStepping = true;
    enableDebugButtons(false);

    function doBatch() {
        if (!isDebugging) { isStepping = false; return; }
        const BATCH_SIZE = 1000;
        for (let i = 0; i < BATCH_SIZE; i++) {
            const running = Module.ccall('wasm_debug_step', 'number', [], []);
            if (!running) {
                isStepping = false;
                updateTui();
                logConsole(Module.ccall('wasm_debug_output', 'string', [], []) + "\n\nProgram execution completed.");
                stopDebugging();
                return;
            }
        }
        updateTui();
        setTimeout(doBatch, 0);
    }
    doBatch();
});

const tabConsole = document.getElementById('tab-console');
const tabDebugger = document.getElementById('tab-debugger');
const contentConsole = document.getElementById('content-console');
const contentDebugger = document.getElementById('content-debugger');

function switchTab(target) {
    if (target === 'console') {
        tabConsole.classList.add('active');
        tabDebugger.classList.remove('active');
        contentConsole.classList.add('active');
        contentDebugger.classList.remove('active');
    } else {
        tabConsole.classList.remove('active');
        tabDebugger.classList.add('active');
        contentConsole.classList.remove('active');
        contentDebugger.classList.add('active');
    }
}

tabConsole.addEventListener('click', () => switchTab('console'));
tabDebugger.addEventListener('click', () => switchTab('debugger'));
