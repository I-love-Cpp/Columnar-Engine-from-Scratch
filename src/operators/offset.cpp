#include "offset.h"
#include "../structs/column.h"
#include <algorithm>

Offset::Offset(std::unique_ptr<IOperator> upstream, size_t offset)
    : upstream_(std::move(upstream)), offset_(offset) {
}

std::optional<Batch> Offset::Next() {
    while (skipped_ < offset_) {
        auto batch_opt = upstream_->Next();
        if (!batch_opt) {
            done_ = true;
            return std::nullopt;
        }

        Batch &batch = *batch_opt;
        if (size_t active = batch.ActiveRows(); skipped_ + active <= offset_) {
            skipped_ += active;
        } else {
            const size_t to_skip = offset_ - skipped_;
            skipped_ = offset_;
            Batch out;
            out.schema = batch.schema;
            const size_t remaining = active - to_skip;
            out.numRows = remaining;
            const auto &sel = batch.HasSelection() ? batch.selection : std::vector<uint32_t>{};
            for (size_t c = 0; c < batch.columns.size(); ++c) {
                auto col = Column::Create(batch.schema.columns[c].typeId);
                for (size_t r = to_skip; r < active; ++r) {
                    size_t phys = batch.HasSelection() ? sel[r] : r;
                    col->PushFromString(batch.columns[c]->GetAsString(phys));
                }

                out.columns.push_back(std::shared_ptr<Column>(col.release()));
            }
            return out;
        }
    }

    return upstream_->Next();
}
