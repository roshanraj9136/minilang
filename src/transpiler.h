#pragma once

#include <string>
#include <vector>
#include "ast.h"

class CPlusPlusTranspiler {
public:
    static std::string transpile(const std::vector<StmtPtr>& statements);
};
