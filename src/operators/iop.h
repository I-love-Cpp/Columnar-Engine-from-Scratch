#pragma once
#include <optional>
#include "../structs/batch.h"

class IOperator {
public:
    virtual std::optional<Batch> Next() = 0;

    virtual ~IOperator() = default;
};
