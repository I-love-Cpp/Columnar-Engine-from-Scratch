#pragma once
#include "iop.h"
#include "../expressions/expression.h"
#include <queue>
#include <vector>

class TopK final : public IOperator {
public:
    struct SortKey {
        std::unique_ptr<Expression> expr;
        bool asc;

        SortKey(std::unique_ptr<Expression> expr, bool asc)
            : expr(std::move(expr)), asc(asc) {
        }

        SortKey(const SortKey &) = delete;

        SortKey &operator=(const SortKey &) = delete;

        SortKey(SortKey &&) = default;

        SortKey &operator=(SortKey &&) = default;
    };

    TopK(std::unique_ptr<IOperator> upstream,
         std::vector<SortKey> sort_keys,
         size_t limit,
         std::vector<std::unique_ptr<Expression> > output_exprs = {});

    std::optional<Batch> Next() override;

private:
    std::unique_ptr<IOperator> upstream_;
    std::vector<SortKey> sort_keys_;
    size_t limit_;
    std::vector<std::unique_ptr<Expression> > output_exprs_;
    bool done_ = false;
    Batch result_;

    struct HeapEntry {
        std::vector<std::string> key_values;
        std::vector<int64_t> numeric_keys;
        std::vector<bool> is_numeric;
        std::vector<std::string> full_row;
        std::vector<std::string> output_row;
        bool has_full_row = false;
    };
};
