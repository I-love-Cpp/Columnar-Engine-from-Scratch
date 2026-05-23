#pragma once
#include "iop.h"
#include "../expressions/expression.h"
#include "../utils/aggregate_utils.h"
#include <vector>
#include <memory>

class GlobalAggregate final : public IOperator {
public:
    enum class AggOp { COUNT, SUM, AVG, MIN, MAX, COUNT_DISTINCT };

    struct Spec {
        AggOp op;
        std::unique_ptr<Expression> arg;

        Spec(const AggOp op, std::unique_ptr<Expression> arg = nullptr)
            : op(op), arg(std::move(arg)) {
        }

        Spec(const Spec &) = delete;

        Spec &operator=(const Spec &) = delete;

        Spec(Spec &&) = default;

        Spec &operator=(Spec &&) = default;
    };

    GlobalAggregate(std::unique_ptr<IOperator> upstream, std::vector<Spec> specs);

    std::optional<Batch> Next() override;

private:
    std::unique_ptr<IOperator> upstream_;
    std::vector<Spec> specs_;
    bool done_ = false;
    Batch result_;
    std::vector<AggState> states_;
};
