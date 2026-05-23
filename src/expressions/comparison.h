#pragma once
#include "expression.h"

enum class CmpOp { EQ, NE, LT, GT, LE, GE };

class Comparison final : public Expression {
    CmpOp op_;
    std::unique_ptr<Expression> left_, right_;

public:
    Comparison(const CmpOp op, std::unique_ptr<Expression> left, std::unique_ptr<Expression> right)
        : op_(op), left_(std::move(left)), right_(std::move(right)) {
    }

    ExprResultType GetResultType() const override {
        return ExprResultType::BOOL;
    }

    std::string EvaluateAsString(const Batch &batch, const size_t row) const override {
        return EvaluateAsBool(batch, row) ? "true" : "false";
    }

    bool EvaluateAsBool(const Batch &batch, const size_t row) const override {
        if (left_->GetResultType() == ExprResultType::INT64 &&
            right_->GetResultType() == ExprResultType::INT64) {
            const int64_t l = left_->EvaluateAsInt64(batch, row);
            const int64_t r = right_->EvaluateAsInt64(batch, row);
            return CompareInt(l, r);
        }
        const std::string l = left_->EvaluateAsString(batch, row);
        const std::string r = right_->EvaluateAsString(batch, row);
        return CompareStr(l, r);
    }

private:
    bool CompareInt(const int64_t l, const int64_t r) const {
        switch (op_) {
            case CmpOp::EQ: return l == r;
            case CmpOp::NE: return l != r;
            case CmpOp::LT: return l < r;
            case CmpOp::GT: return l > r;
            case CmpOp::LE: return l <= r;
            case CmpOp::GE: return l >= r;
        }
        return false;
    }

    bool CompareStr(const std::string &l, const std::string &r) const {
        const int cmp = l.compare(r);
        switch (op_) {
            case CmpOp::EQ: return cmp == 0;
            case CmpOp::NE: return cmp != 0;
            case CmpOp::LT: return cmp < 0;
            case CmpOp::GT: return cmp > 0;
            case CmpOp::LE: return cmp <= 0;
            case CmpOp::GE: return cmp >= 0;
        }
        return false;
    }
};
