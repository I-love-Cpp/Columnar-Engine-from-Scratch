#pragma once
#include "iop.h"
#include "../structs/columnar_format.h"
#include <vector>

class TableScan final : public IOperator {
    ColumnarReader &reader_;
    std::vector<size_t> column_indices_;
    size_t next_rg_ = 0;

public:
    TableScan(ColumnarReader &reader, std::vector<size_t> column_indices);

    std::optional<Batch> Next() override;
};
