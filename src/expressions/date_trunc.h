#pragma once
#include "expression.h"

class DateTruncMinuteFunction : public Expression {
    std::unique_ptr<Expression> arg_;

public:
    explicit DateTruncMinuteFunction(std::unique_ptr<Expression> arg) : arg_(std::move(arg)) {
    }

    ExprResultType GetResultType() const override {
        return ExprResultType::INT64;
    }

    std::string EvaluateAsString(const Batch &batch, size_t row) const override {
        return std::to_string(EvaluateAsInt64(batch, row));
    }

    int64_t EvaluateAsInt64(const Batch &batch, size_t row) const override {
        const int64_t micros = arg_->EvaluateAsInt64(batch, row);
        constexpr int64_t minute_micros = 60LL * 1000000LL;
        return (micros / minute_micros) * minute_micros;
    }
};
