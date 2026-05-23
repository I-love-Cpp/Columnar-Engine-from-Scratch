#pragma once
#include "expression.h"

class IfFunction final : public Expression {
    std::unique_ptr<Expression> cond_, then_expr_, else_expr_;

public:
    IfFunction(std::unique_ptr<Expression> cond,
               std::unique_ptr<Expression> then_expr,
               std::unique_ptr<Expression> else_expr)
        : cond_(std::move(cond)), then_expr_(std::move(then_expr)), else_expr_(std::move(else_expr)) {
    }

    ExprResultType GetResultType() const override {
        return then_expr_->GetResultType();
    }

    std::string EvaluateAsString(const Batch &batch, const size_t row) const override {
        return cond_->EvaluateAsBool(batch, row)
                   ? then_expr_->EvaluateAsString(batch, row)
                   : else_expr_->EvaluateAsString(batch, row);
    }

    int64_t EvaluateAsInt64(const Batch &batch, const size_t row) const override {
        return cond_->EvaluateAsBool(batch, row)
                   ? then_expr_->EvaluateAsInt64(batch, row)
                   : else_expr_->EvaluateAsInt64(batch, row);
    }

    bool EvaluateAsBool(const Batch &batch, const size_t row) const override {
        return cond_->EvaluateAsBool(batch, row)
                   ? then_expr_->EvaluateAsBool(batch, row)
                   : else_expr_->EvaluateAsBool(batch, row);
    }
};
