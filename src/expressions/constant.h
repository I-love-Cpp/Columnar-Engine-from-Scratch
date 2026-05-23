#pragma once
#include "expression.h"

class Constant final : public Expression {
    std::string str_val_;
    int64_t int_val_ = 0;
    ExprResultType type_;

public:
    explicit Constant(std::string value) : str_val_(std::move(value)), type_(ExprResultType::STRING) {
    }

    explicit Constant(int64_t value) : int_val_(value), type_(ExprResultType::INT64) {
    }

    ExprResultType GetResultType() const override {
        return type_;
    }

    std::string EvaluateAsString(const Batch &, size_t) const override {
        if (type_ == ExprResultType::INT64) {
            return std::to_string(int_val_);
        }
        return str_val_;
    }

    int64_t EvaluateAsInt64(const Batch &, size_t) const override {
        if (type_ == ExprResultType::INT64) {
            return int_val_;
        }
        return std::stoll(str_val_);
    }
};
