#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <stdexcept>
#include "token.h"
#include "lexer.h"
#include "ast.h"
#include "parser.h"
#include "semantic.h"
#include "codegen.h"
#include "opcode.h"
#include "vm.h"
#include "disassembler.h"
#include "debugger.h"
#include "error.h"
#include "transpiler.h"
#include "optimizer.h"

std::string read_file(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        throw std::runtime_error("Could not open file: " + path);
    }
    std::stringstream ss;
    ss << file.rdbuf();
    return ss.str();
}

std::vector<Token> tokenize(Lexer& lexer) {
    std::vector<Token> tokens;
    while (true) {
        Token tok = lexer.next_token();
        tokens.push_back(tok);
        if (tok.type == TokenType::TOKEN_EOF || tok.type == TokenType::TOKEN_ERROR) {
            break;
        }
    }
    return tokens;
}

int compile_and_run(const std::string& source) {
    ErrorReporter errors;
    Lexer lexer(source);
    std::vector<Token> tokens = tokenize(lexer);

    for (const auto& tok : tokens) {
        if (tok.type == TokenType::TOKEN_ERROR) {
            errors.report_error(source, tok.line, tok.column, tok.lexeme);
            return 1;
        }
    }

    Parser parser(tokens, errors, source);
    std::vector<StmtPtr> ast;
    try {
        ast = parser.parse();
    } catch (const std::exception& e) {
        std::cerr << "Parser crashed: " << e.what() << "\n";
        return 1;
    }

    if (errors.has_errors()) return 1;

    SemanticAnalyzer semantic(errors, source);
    semantic.analyze(ast);
    Optimizer::optimize(ast);

    if (errors.has_errors()) return 1;

    CodeGenerator codegen(errors, source);
    CompiledProgram program = codegen.generate(ast);

    if (errors.has_errors()) return 1;

    VM vm(program);
    VMResult res = vm.run();
    if (res != VMResult::OK) return 1;

    return 0;
}

int compile_and_debug(const std::string& source) {
    ErrorReporter errors;
    Lexer lexer(source);
    std::vector<Token> tokens = tokenize(lexer);

    for (const auto& tok : tokens) {
        if (tok.type == TokenType::TOKEN_ERROR) {
            errors.report_error(source, tok.line, tok.column, tok.lexeme);
            return 1;
        }
    }

    Parser parser(tokens, errors, source);
    std::vector<StmtPtr> ast;
    try {
        ast = parser.parse();
    } catch (...) {
        return 1;
    }

    if (errors.has_errors()) return 1;

    SemanticAnalyzer semantic(errors, source);
    semantic.analyze(ast);
    Optimizer::optimize(ast);

    if (errors.has_errors()) return 1;

    CodeGenerator codegen(errors, source);
    CompiledProgram program = codegen.generate(ast);

    if (errors.has_errors()) return 1;

    Debugger dbg(source, program);
    dbg.run();
    return 0;
}

void dump_tokens(const std::string& source) {
    Lexer lexer(source);
    std::vector<Token> tokens = tokenize(lexer);
    for (const auto& tok : tokens) {
        std::cout << "[line " << tok.line << ", col " << tok.column << "] "
                  << token_type_to_string(tok.type) << " '" << tok.lexeme << "'\n";
    }
}

void dump_ast(const std::string& source) {
    ErrorReporter errors;
    Lexer lexer(source);
    std::vector<Token> tokens = tokenize(lexer);
    Parser parser(tokens, errors, source);
    std::vector<StmtPtr> ast = parser.parse();
    for (const auto& stmt : ast) {
        std::cout << stmt_to_string(*stmt, 0) << "\n";
    }
}

void dump_bytecode(const std::string& source) {
    ErrorReporter errors;
    Lexer lexer(source);
    std::vector<Token> tokens = tokenize(lexer);
    Parser parser(tokens, errors, source);
    std::vector<StmtPtr> ast = parser.parse();
    SemanticAnalyzer semantic(errors, source);
    semantic.analyze(ast);
    Optimizer::optimize(ast);
    CodeGenerator codegen(errors, source);
    CompiledProgram program = codegen.generate(ast);
    std::cout << Disassembler::disassemble(program);
}

void run_repl() {
    std::cout << "MiniLang REPL v1.0\nType expressions or statements. Empty line executes. 'quit' exits.\n\n";
    std::string accumulated = "";
    while (true) {
        std::cout << (accumulated.empty() ? ">>> " : "... ");
        std::string line;
        if (!std::getline(std::cin, line)) break;
        if (line == "quit" || line == "exit") break;
        if (line.empty() && !accumulated.empty()) {
            std::string code = accumulated;
            while (!code.empty() && (code.back() == '\n' || code.back() == '\r' || code.back() == ' ' || code.back() == '\t')) {
                code.pop_back();
            }
            if (code.empty()) {
                accumulated.clear();
                continue;
            }
            if (code.find("fn ") == std::string::npos) {
                bool is_expr = code.find("let ") == std::string::npos &&
                               code.find("if ") == std::string::npos &&
                               code.find("while ") == std::string::npos &&
                               code.find("for ") == std::string::npos &&
                               code.find("print") == std::string::npos &&
                               code.find("return") == std::string::npos;
                if (is_expr && code.back() != ';') {
                    code = "fn main() { print(" + code + "); }";
                } else {
                    code = "fn main() {\n" + code + "\n}";
                }
            }
            compile_and_run(code);
            accumulated.clear();
        } else {
            accumulated += line + "\n";
        }
    }
}

int compile_to_native(const std::string& source, const std::string& output_path, bool generate_assembly) {
    ErrorReporter errors;
    Lexer lexer(source);
    std::vector<Token> tokens = tokenize(lexer);

    for (const auto& tok : tokens) {
        if (tok.type == TokenType::TOKEN_ERROR) {
            errors.report_error(source, tok.line, tok.column, tok.lexeme);
            return 1;
        }
    }

    Parser parser(tokens, errors, source);
    std::vector<StmtPtr> ast;
    try {
        ast = parser.parse();
    } catch (const std::exception& e) {
        std::cerr << "Parser crashed: " << e.what() << "\n";
        return 1;
    }

    if (errors.has_errors()) return 1;

    SemanticAnalyzer semantic(errors, source);
    semantic.analyze(ast);
    Optimizer::optimize(ast);

    if (errors.has_errors()) return 1;

    std::string cpp_code = CPlusPlusTranspiler::transpile(ast);

    std::string temp_cpp_path = "temp_transpiled.cpp";
    std::ofstream temp_file(temp_cpp_path);
    if (!temp_file.is_open()) {
        std::cerr << "Could not create temporary file for compilation\n";
        return 1;
    }
    temp_file << cpp_code;
    temp_file.close();

    std::string cmd = "g++ -O3 -std=c++17 ";
    if (generate_assembly) {
        cmd += "-S ";
    }
    cmd += temp_cpp_path + " -o " + output_path;

    int compile_res = std::system(cmd.c_str());

    std::remove(temp_cpp_path.c_str());

    if (compile_res != 0) {
        std::cerr << "Native compilation failed\n";
        return 1;
    }

    return 0;
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        run_repl();
        return 0;
    }

    std::string mode = argv[1];
    if (mode == "repl") {
        run_repl();
        return 0;
    }

    if (mode == "--help" || mode == "-h") {
        std::cout << "MiniLang Compiler v1.0 (GCC-Powered Native Compiler)\n"
                  << "Usage:\n"
                  << "  minilang compile <file.ml> [-o <out>] [-S] Compile to native binary / assembly\n"
                  << "  minilang run <file.ml>                     Compile and execute on VM\n"
                  << "  minilang debug <file.ml>                   Debug with visual TUI\n"
                  << "  minilang disasm <file.ml>                  Disassemble bytecode\n"
                  << "  minilang repl                              Interactive REPL\n"
                  << "  minilang --dump-tokens <file>              Show tokens\n"
                  << "  minilang --dump-ast <file>                 Show AST\n"
                  << "  minilang --help                            Show this help\n";
        return 0;
    }

    if (mode == "compile") {
        if (argc < 3) {
            std::cerr << "Usage: minilang compile <file.ml> [-o <output>] [-S]\n";
            return 1;
        }
        std::string file_path = argv[2];
        std::string output_path = "a.out";
        bool generate_assembly = false;

        for (int i = 3; i < argc; ++i) {
            std::string arg = argv[i];
            if (arg == "-o" && i + 1 < argc) {
                output_path = argv[++i];
            } else if (arg == "-S") {
                generate_assembly = true;
                if (output_path == "a.out") {
                    output_path = "a.s";
                }
            }
        }

        std::string source;
        try {
            source = read_file(file_path);
        } catch (const std::exception& e) {
            std::cerr << e.what() << "\n";
            return 1;
        }
        return compile_to_native(source, output_path, generate_assembly);
    }

    if (argc < 3) {
        std::string source;
        try {
            source = read_file(mode);
        } catch (const std::exception& e) {
            std::cerr << e.what() << "\n";
            return 1;
        }
        return compile_and_run(source);
    }

    std::string file_path = argv[2];
    std::string source;
    try {
        source = read_file(file_path);
    } catch (const std::exception& e) {
        std::cerr << e.what() << "\n";
        return 1;
    }

    if (mode == "run") {
        return compile_and_run(source);
    } else if (mode == "debug") {
        return compile_and_debug(source);
    } else if (mode == "disasm") {
        dump_bytecode(source);
        return 0;
    } else if (mode == "--dump-tokens") {
        dump_tokens(source);
        return 0;
    } else if (mode == "--dump-ast") {
        dump_ast(source);
        return 0;
    }

    std::cerr << "Unknown mode: " << mode << "\n";
    return 1;
}
