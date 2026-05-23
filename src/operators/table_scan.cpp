#include "table_scan.h"

TableScan::TableScan(ColumnarReader &reader, std::vector<size_t> column_indices)
    : reader_(reader), column_indices_(std::move(column_indices)) {
}

std::optional<Batch> TableScan::Next() {
    if (next_rg_ >= reader_.NumRowGroups()) {
        return std::nullopt;
    }

    RowGroup rg = reader_.ReadRowGroupColumns(next_rg_, column_indices_);
    Batch batch;
    for (size_t i = 0; i < column_indices_.size(); ++i) {
        const auto &col_def = reader_.GetSchema().columns[column_indices_[i]];
        batch.schema.columns.push_back(col_def);
    }

    batch.columns = std::move(rg.columns);
    batch.numRows = rg.numRows;
    size_t min_rows = batch.numRows;
    for (const auto &col: batch.columns) {
        if (col->Size() < min_rows) {
            min_rows = col->Size();
        }
    }

    if (min_rows == 0 && batch.numRows > 0) {
        ++next_rg_;
        return Next();
    }

    batch.numRows = min_rows;
    ++next_rg_;
    return batch;
}
