#include "sort.h"
#include "../structs/column.h"
#include <algorithm>
#include <cstdlib>

Sort::Sort(std::unique_ptr<IOperator> upstream, std::vector<SortKey> sort_keys)
    : upstream_(std::move(upstream)), sort_keys_(std::move(sort_keys)) {
}

std::optional<Batch> Sort::Next() {
    if (done_) {
        return std::nullopt;
    }

    done_ = true;
    std::vector<Batch> temp_batches;
    size_t total_rows = 0;
    Schema schema;
    bool schema_set = false;
    while (auto batch_opt = upstream_->Next()) {
        Batch &b = *batch_opt;
        total_rows += b.ActiveRows();
        if (!schema_set) {
            schema = b.schema;
            schema_set = true;
        }

        temp_batches.push_back(std::move(b));
    }

    if (total_rows == 0) {
        result_.numRows = 0;
        return result_;
    }

    const size_t num_cols = schema.NumColumns();
    std::vector all_rows(total_rows, std::vector<std::string>(num_cols));
    std::vector<size_t> indices(total_rows);
    size_t row_idx = 0;
    for (const auto &batch: temp_batches) {
        size_t rows = batch.ActiveRows();
        const auto &sel = batch.HasSelection() ? batch.selection : std::vector<uint32_t>{};
        for (size_t r = 0; r < rows; ++r) {
            const size_t phys = batch.HasSelection() ? sel[r] : r;
            for (size_t c = 0; c < num_cols; ++c) {
                all_rows[row_idx][c] = batch.columns[c]->GetAsString(phys);
            }
            indices[row_idx] = row_idx;
            ++row_idx;
        }
    }

    auto comparator = [&](const size_t a, const size_t b) -> bool {
        for (const auto &key: sort_keys_) {
            const size_t col_idx = key.column_index;
            const std::string &sa = all_rows[a][col_idx];
            const std::string &sb = all_rows[b][col_idx];
            const bool asc = key.asc;
            TypesId type_id = schema.columns[col_idx].typeId;
            if (type_id == TypesId::int64 || type_id == TypesId::int32 || type_id == TypesId::int16) {
                int64_t va = std::stoll(sa);
                int64_t vb = std::stoll(sb);
                if (va != vb) {
                    return asc ? (va < vb) : (va > vb);
                }
            } else {
                int cmp = sa.compare(sb);
                if (cmp != 0) {
                    return asc ? (cmp < 0) : (cmp > 0);
                }
            }
        }
        return false;
    };

    std::ranges::sort(indices, comparator);
    result_.schema = schema;
    result_.numRows = total_rows;
    for (size_t c = 0; c < num_cols; ++c) {
        auto col = Column::Create(schema.columns[c].typeId);
        for (size_t i: indices) {
            col->PushFromString(all_rows[i][c]);
        }

        result_.columns.push_back(std::shared_ptr<Column>(col.release()));
    }

    return result_;
}
