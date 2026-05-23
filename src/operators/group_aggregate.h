#pragma once
#include "iop.h"
#include "../expressions/expression.h"
#include "../utils/aggregate_utils.h"
#include <vector>
#include <memory>
#include <unordered_map>
#include <string>

#include "global_aggregate.h"

class GroupAggregate final : public IOperator {
public:
    using AggOp = GlobalAggregate::AggOp;

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

    GroupAggregate(std::unique_ptr<IOperator> upstream,
                   std::vector<std::unique_ptr<Expression> > key_exprs,
                   std::vector<Spec> agg_specs);

    std::optional<Batch> Next() override;

private:
    std::unique_ptr<IOperator> upstream_;
    std::vector<std::unique_ptr<Expression> > key_exprs_;
    std::vector<Spec> agg_specs_;
    bool done_ = false;
    std::vector<Batch> result_batches_;
    size_t result_idx_ = 0;

    std::unordered_map<std::string, std::vector<AggState> > groups_;
};
