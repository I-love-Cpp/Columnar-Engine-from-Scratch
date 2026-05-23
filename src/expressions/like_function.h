#pragma once
#include "expression.h"

class LikeFunction final : public Expression {
    std::unique_ptr<Expression> arg_;
    std::string pattern_;

public:
    LikeFunction(std::unique_ptr<Expression> arg, std::string pattern)
        : arg_(std::move(arg)), pattern_(std::move(pattern)) {
    }

    ExprResultType GetResultType() const override {
        return ExprResultType::BOOL;
    }

    std::string EvaluateAsString(const Batch &batch, const size_t row) const override {
        return EvaluateAsBool(batch, row) ? "true" : "false";
    }

    bool EvaluateAsBool(const Batch &batch, const size_t row) const override {
        std::string s = arg_->EvaluateAsString(batch, row);
        std::string pat = pattern_;
        if (!pat.empty() && pat.front() == '%') {
            pat.erase(0, 1);
        }
        if (!pat.empty() && pat.back() == '%') {
            pat.pop_back();
        }
        return s.find(pat) != std::string::npos;
    }
};
