#pragma once
#include "iop.h"
#include <memory>

class Offset final : public IOperator {
    std::unique_ptr<IOperator> upstream_;
    size_t offset_;
    size_t skipped_ = 0;
    bool done_ = false;

public:
    Offset(std::unique_ptr<IOperator> upstream, size_t offset);

    std::optional<Batch> Next() override;
};
