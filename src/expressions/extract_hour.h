#pragma once
#include "expression.h"

class ExtractHourFunction final : public Expression {
    std::unique_ptr<Expression> arg_;

public:
    explicit ExtractHourFunction(std::unique_ptr<Expression> arg) : arg_(std::move(arg)) {
    }

    ExprResultType GetResultType() const override {
        return ExprResultType::INT64;
    }

    std::string EvaluateAsString(const Batch &batch, const size_t row) const override {
        return std::to_string(EvaluateAsInt64(batch, row));
    }

    int64_t EvaluateAsInt64(const Batch &batch, const size_t row) const override {
        const int64_t micros = arg_->EvaluateAsInt64(batch, row);
        return (micros / 3'600'000'000LL) % 24;
    }
};
