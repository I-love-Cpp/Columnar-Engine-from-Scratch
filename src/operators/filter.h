#pragma once
#include "iop.h"
#include "../expressions/expression.h"
#include <memory>

class Filter final : public IOperator {
    std::unique_ptr<IOperator> upstream_;
    std::unique_ptr<Expression> predicate_;

public:
    Filter(std::unique_ptr<IOperator> upstream, std::unique_ptr<Expression> predicate);

    std::optional<Batch> Next() override;
};
