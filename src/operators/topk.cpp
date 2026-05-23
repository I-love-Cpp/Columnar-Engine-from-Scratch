#include "topk.h"
#include "../structs/column.h"
#include <algorithm>

TopK::TopK(std::unique_ptr<IOperator> upstream,
           std::vector<SortKey> sort_keys,
           const size_t limit,
           std::vector<std::unique_ptr<Expression> > output_exprs)
    : upstream_(std::move(upstream)), sort_keys_(std::move(sort_keys)),
      limit_(limit), output_exprs_(std::move(output_exprs)) {
}

std::optional<Batch> TopK::Next() {
    if (done_) {
        return std::nullopt;
    }

    done_ = true;
    auto comp = [this](const HeapEntry &a, const HeapEntry &b) -> bool {
        for (size_t i = 0; i < sort_keys_.size(); ++i) {
            const bool asc = sort_keys_[i].asc;
            const bool a_num = a.is_numeric[i];
            const bool b_num = b.is_numeric[i];
            if (a_num && b_num) {
                const int64_t va = a.numeric_keys[i];
                const int64_t vb = b.numeric_keys[i];
                if (va != vb) {
                    return asc ? (va < vb) : (va > vb);
                }
            } else {
                int cmp = a.key_values[i].compare(b.key_values[i]);
                if (cmp != 0) {
                    return asc ? (cmp < 0) : (cmp > 0);
                }
            }
        }
        return false;
    };

    std::priority_queue<HeapEntry, std::vector<HeapEntry>, decltype(comp)> heap(comp);
    bool schema_init = false;
    Schema schema;
    while (auto batch_opt = upstream_->Next()) {
        Batch &batch = *batch_opt;
        size_t rows = batch.ActiveRows();
        if (rows == 0) {
            continue;
        }

        if (!schema_init && output_exprs_.empty()) {
            schema = batch.schema;
            schema_init = true;
        }

        for (size_t r = 0; r < rows; ++r) {
            HeapEntry entry;
            entry.key_values.resize(sort_keys_.size());
            entry.numeric_keys.resize(sort_keys_.size(), 0);
            entry.is_numeric.resize(sort_keys_.size(), false);

            for (size_t i = 0; i < sort_keys_.size(); ++i) {
                auto &key = sort_keys_[i];
                if (key.expr->GetResultType() == ExprResultType::INT64) {
                    int64_t val = key.expr->EvaluateAsInt64(batch, r);
                    entry.numeric_keys[i] = val;
                    entry.key_values[i] = std::to_string(val);
                    entry.is_numeric[i] = true;
                } else {
                    entry.key_values[i] = key.expr->EvaluateAsString(batch, r);
                }
            }

            if (output_exprs_.empty()) {
                for (size_t c = 0; c < batch.columns.size(); ++c) {
                    entry.full_row.push_back(batch.columns[c]->GetAsString(
                        batch.HasSelection() ? batch.selection[r] : r));
                }
                entry.has_full_row = true;
            } else {
                for (const auto &expr: output_exprs_) {
                    entry.output_row.push_back(expr->EvaluateAsString(batch, r));
                }
            }

            heap.push(std::move(entry));
            if (heap.size() > limit_) {
                heap.pop();
            }
        }
    }

    std::vector<HeapEntry> sorted;
    while (!heap.empty()) {
        sorted.push_back(std::move(const_cast<HeapEntry &>(heap.top())));
        heap.pop();
    }

    std::ranges::reverse(sorted);
    if (output_exprs_.empty()) {
        if (!schema_init || sorted.empty()) {
            result_.numRows = 0;
            return result_;
        }
        result_.schema = schema;
        for (size_t c = 0; c < schema.NumColumns(); ++c) {
            auto col = Column::Create(schema.columns[c].typeId);
            for (const auto &entry: sorted) {
                col->PushFromString(entry.full_row[c]);
            }
            result_.columns.push_back(std::shared_ptr<Column>(col.release()));
        }
        result_.numRows = sorted.size();
    } else {
        for (size_t c = 0; c < output_exprs_.size(); ++c) {
            result_.schema.columns.push_back({"out" + std::to_string(c), TypesId::string});
            auto col = Column::Create(TypesId::string);
            for (const auto &entry: sorted) {
                col->PushFromString(entry.output_row[c]);
            }

            result_.columns.push_back(std::shared_ptr<Column>(col.release()));
        }

        result_.numRows = sorted.size();
    }
    return result_;
}
