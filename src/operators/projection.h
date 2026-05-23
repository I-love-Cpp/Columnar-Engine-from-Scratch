#pragma once
#include "iop.h"
#include "../expressions/expression.h"
#include <vector>
#include <memory>

class Projection final : public IOperator {
    std::unique_ptr<IOperator> upstream_;
    std::vector<std::unique_ptr<Expression> > exprs_;
    std::vector<std::string> aliases_;

public:
    Projection(std::unique_ptr<IOperator> upstream,
               std::vector<std::unique_ptr<Expression> > exprs,
               std::vector<std::string> aliases);

    std::optional<Batch> Next() override;
};
