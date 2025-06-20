#pragma once

#include "exprs/expr.h"

namespace starrocks {

class TExprNode;

class Predicate : public Expr {
public:
    virtual Status merge(Predicate* predicate) { return Status::NotSupported("Not supported"); }

protected:
    friend class Expr;
    Predicate(const TExprNode& node) : Expr(node) {}
};

} // namespace starrocks
