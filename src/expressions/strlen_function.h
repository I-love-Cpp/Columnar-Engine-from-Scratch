#pragma once
#include "expression.h"

class StrLenFunction final : public Expression {
    std::unique_ptr<Expression> arg_;

public:
    explicit StrLenFunction(std::unique_ptr<Expression> arg) : arg_(std::move(arg)) {
    }

    ExprResultType GetResultType() const override {
        return ExprResultType::INT64;
    }

    std::string EvaluateAsString(const Batch &batch, const size_t row) const override {
        return std::to_string(EvaluateAsInt64(batch, row));
    }

    int64_t EvaluateAsInt64(const Batch &batch, const size_t row) const override {
        return static_cast<int64_t>(arg_->EvaluateAsString(batch, row).size());
    }
};
