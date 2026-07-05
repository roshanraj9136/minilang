#pragma once

#include <iostream>
#include <string>
#include <vector>
#include <sstream>

namespace Color {
#ifdef __EMSCRIPTEN__
    inline const std::string RESET = "";
    inline const std::string RED = "";
    inline const std::string YELLOW = "";
    inline const std::string CYAN = "";
    inline const std::string BOLD = "";
    inline const std::string DIM = "";
#else
    inline const std::string RESET = "\033[0m";
    inline const std::string RED = "\033[31m";
    inline const std::string YELLOW = "\033[33m";
    inline const std::string CYAN = "\033[36m";
    inline const std::string BOLD = "\033[1m";
    inline const std::string DIM = "\033[2m";
#endif
}

class ErrorReporter {
public:
    void report_error(const std::string& source, int line, int column, const std::string& message) {
        report(source, line, column, "error", Color::RED, message);
        error_count_++;
    }

    void report_warning(const std::string& source, int line, int column, const std::string& message) {
        report(source, line, column, "warning", Color::YELLOW, message);
    }

    bool has_errors() const {
        return error_count_ > 0;
    }

    int error_count() const {
        return error_count_;
    }

    void reset() {
        error_count_ = 0;
    }

private:
    int error_count_ = 0;

    void report(const std::string& source, int line, int column, const std::string& level, const std::string& color, const std::string& message) {
        std::vector<std::string> lines;
        std::string current_line;
        std::istringstream stream(source);
        while (std::getline(stream, current_line)) {
            lines.push_back(current_line);
        }

        std::cerr << Color::BOLD << color << level << Color::RESET
                  << Color::BOLD << "[line " << line << ", col " << column << "]: " << Color::RESET
                  << message << "\n\n";

        if (line > 0 && line <= static_cast<int>(lines.size())) {
            std::string source_line = lines[line - 1];
            std::cerr << "    " << source_line << "\n";
            std::cerr << "    ";
            for (int i = 1; i < column; ++i) {
                if (i - 1 < static_cast<int>(source_line.size()) && source_line[i - 1] == '\t') {
                    std::cerr << "\t";
                } else {
                    std::cerr << " ";
                }
            }
            std::cerr << Color::BOLD << color << "^" << Color::RESET << "\n\n";
        }
    }
};
