#pragma once

#include <vector>
#include "ast.h"

class Optimizer {
public:
    static void optimize(std::vector<StmtPtr>& statements);

private:
    static void optimize_stmt(Stmt& stmt);
    static ExprPtr optimize_expr(ExprPtr expr);
};
