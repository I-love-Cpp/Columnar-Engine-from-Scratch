#pragma once
#include "iop.h"
#include <memory>

class Limit final : public IOperator {
    std::unique_ptr<IOperator> upstream_;
    size_t limit_;
    size_t returned_ = 0;
    bool done_ = false;
public:
    Limit(std::unique_ptr<IOperator> upstream, size_t limit);

    std::optional<Batch> Next() override;
};