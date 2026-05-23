#include "limit.h"

Limit::Limit(std::unique_ptr<IOperator> upstream, size_t limit)
    : upstream_(std::move(upstream)), limit_(limit) {
}

std::optional<Batch> Limit::Next() {
    if (done_ || returned_ >= limit_) {
        return std::nullopt;
    }

    auto batch_opt = upstream_->Next();
    if (!batch_opt) {
        done_ = true;
        return std::nullopt;
    }

    Batch &batch = *batch_opt;
    if (size_t active = batch.ActiveRows(); returned_ + active <= limit_) {
        returned_ += active;
        return batch;
    }

    const size_t need = limit_ - returned_;
    returned_ = limit_;
    done_ = true;
    Batch out;
    out.schema = batch.schema;
    for (size_t c = 0; c < batch.columns.size(); ++c) {
        auto col = Column::Create(batch.schema.columns[c].typeId);
        const auto &sel = batch.HasSelection() ? batch.selection : std::vector<uint32_t>{};
        for (size_t i = 0; i < need; ++i) {
            const size_t phys = batch.HasSelection() ? sel[i] : i;
            col->PushFromString(batch.columns[c]->GetAsString(phys));
        }
        out.columns.push_back(std::shared_ptr<Column>(col.release()));
    }

    out.numRows = need;
    return out;
}
