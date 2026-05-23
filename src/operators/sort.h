#pragma once
#include "iop.h"
#include <vector>

class Sort final : public IOperator {
public:
    struct SortKey {
        size_t column_index;
        bool asc;
    };

    Sort(std::unique_ptr<IOperator> upstream, std::vector<SortKey> sort_keys);

    std::optional<Batch> Next() override;

private:
    std::unique_ptr<IOperator> upstream_;
    std::vector<SortKey> sort_keys_;
    bool done_ = false;
    Batch result_;
};
