#pragma once
#include "expression.h"

enum class ArithOp { PLUS, MINUS, MULT, DIV };

class BinaryArithmetic final : public Expression {
    ArithOp op_;
    std::unique_ptr<Expression> left_, right_;

public:
    BinaryArithmetic(const ArithOp op, std::unique_ptr<Expression> left, std::unique_ptr<Expression> right)
        : op_(op), left_(std::move(left)), right_(std::move(right)) {
    }

    ExprResultType GetResultType() const override {
        return ExprResultType::INT64;
    }

    std::string EvaluateAsString(const Batch &batch, const size_t row) const override {
        return std::to_string(EvaluateAsInt64(batch, row));
    }

    int64_t EvaluateAsInt64(const Batch &batch, const size_t row) const override {
        int64_t l = left_->EvaluateAsInt64(batch, row);
        int64_t r = right_->EvaluateAsInt64(batch, row);
        switch (op_) {
            case ArithOp::PLUS: return l + r;
            case ArithOp::MINUS: return l - r;
            case ArithOp::MULT: return l * r;
            case ArithOp::DIV: return r ? l / r : 0;
        }
        return 0;
    }
};
