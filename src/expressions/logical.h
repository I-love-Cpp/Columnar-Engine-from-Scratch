#pragma once
#include "expression.h"
#include <vector>
#include <memory>

class LogicalAnd final : public Expression {
    std::vector<std::unique_ptr<Expression> > operands_;

public:
    explicit LogicalAnd(std::vector<std::unique_ptr<Expression> > ops)
        : operands_(std::move(ops)) {
    }

    ExprResultType GetResultType() const override {
        return ExprResultType::BOOL;
    }

    std::string EvaluateAsString(const Batch &batch, const size_t row) const override {
        return EvaluateAsBool(batch, row) ? "true" : "false";
    }

    bool EvaluateAsBool(const Batch &batch, const size_t row) const override {
        for (const auto &op: operands_) {
            if (!op->EvaluateAsBool(batch, row)) {
                return false;
            }
        }
        return true;
    }
};

class LogicalOr final : public Expression {
    std::vector<std::unique_ptr<Expression> > operands_;

public:
    explicit LogicalOr(std::vector<std::unique_ptr<Expression> > ops)
        : operands_(std::move(ops)) {
    }

    ExprResultType GetResultType() const override {
        return ExprResultType::BOOL;
    }

    std::string EvaluateAsString(const Batch &batch, const size_t row) const override {
        return EvaluateAsBool(batch, row) ? "true" : "false";
    }

    bool EvaluateAsBool(const Batch &batch, size_t row) const override {
        for (const auto &op: operands_) {
            if (op->EvaluateAsBool(batch, row)) return true;
        }
        return false;
    }
};

class LogicalNot : public Expression {
    std::unique_ptr<Expression> operand_;

public:
    explicit LogicalNot(std::unique_ptr<Expression> op)
        : operand_(std::move(op)) {
    }

    ExprResultType GetResultType() const override {
        return ExprResultType::BOOL;
    }

    std::string EvaluateAsString(const Batch &batch, const size_t row) const override {
        return EvaluateAsBool(batch, row) ? "true" : "false";
    }

    bool EvaluateAsBool(const Batch &batch, const size_t row) const override {
        return !operand_->EvaluateAsBool(batch, row);
    }
};
