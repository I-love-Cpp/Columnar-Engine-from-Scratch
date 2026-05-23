#pragma once
#include "iop.h"
#include "../structs/batch.h"

class MaterializedScan final : public IOperator {
    Batch data_;
    bool given_ = false;

public:
    explicit MaterializedScan(Batch batch) : data_(std::move(batch)) {
    }

    std::optional<Batch> Next() override {
        if (given_) {
            return std::nullopt;
        }
        given_ = true;
        if (data_.numRows == 0) {
            return std::nullopt;
        }
        return data_;
    }
};
