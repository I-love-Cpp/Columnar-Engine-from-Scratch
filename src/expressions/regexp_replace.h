#pragma once
#include "expression.h"

class RegexpReplaceFunction final : public Expression {
    std::unique_ptr<Expression> arg_;
    std::string replacement_;

public:
    RegexpReplaceFunction(std::unique_ptr<Expression> arg, const std::string &,
                          const std::string &replacement)
        : arg_(std::move(arg)), replacement_(replacement) {
    }

    ExprResultType GetResultType() const override {
        return ExprResultType::STRING;
    }

    std::string EvaluateAsString(const Batch &batch, const size_t row) const override {
        std::string url = arg_->EvaluateAsString(batch, row);
        size_t pos = url.find("://");
        if (pos == std::string::npos) {
            return "";
        }

        pos += 3;
        if (url.compare(pos, 4, "www.") == 0) {
            pos += 4;
        }

        size_t end = url.find('/', pos);
        if (end == std::string::npos) {
            end = url.size();
        }

        std::string domain = url.substr(pos, end - pos);
        if (replacement_ == "\\1") {
            return domain;
        }
        return domain;
    }
};
