#pragma once
#include <string>
#include "../structs/batch.h"

enum class ExprResultType { STRING, INT64, BOOL };

class Expression {
public:
    virtual ~Expression() = default;

    virtual ExprResultType GetResultType() const = 0;

    virtual std::string EvaluateAsString(const Batch &batch, size_t row) const = 0;

    virtual int64_t EvaluateAsInt64(const Batch &batch, const size_t row) const {
        return std::stoll(EvaluateAsString(batch, row));
    }

    virtual bool EvaluateAsBool(const Batch &batch, const size_t row) const {
        return EvaluateAsString(batch, row) == "true";
    }
};
