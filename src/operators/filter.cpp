#include "filter.h"

Filter::Filter(std::unique_ptr<IOperator> upstream, std::unique_ptr<Expression> predicate)
    : upstream_(std::move(upstream)), predicate_(std::move(predicate)) {
}

std::optional<Batch> Filter::Next() {
    auto batch_opt = upstream_->Next();
    if (!batch_opt) {
        return std::nullopt;
    }

    Batch &batch = *batch_opt;
    static constexpr std::vector<uint32_t> empty_selection;
    const std::vector<uint32_t> &cur = batch.HasSelection() ? batch.selection : empty_selection;
    const size_t total = batch.HasSelection() ? cur.size() : batch.numRows;
    std::vector<uint32_t> new_sel;
    new_sel.reserve(total);
    for (size_t i = 0; i < total; ++i) {
        size_t phys = batch.HasSelection() ? cur[i] : i;
        if (predicate_->EvaluateAsBool(batch, phys)) {
            new_sel.push_back(static_cast<uint32_t>(phys));
        }
    }

    if (new_sel.empty()) {
        batch.numRows = 0;
        batch.selection.clear();
    } else {
        batch.numRows = new_sel.size();
        batch.selection = std::move(new_sel);
    }

    return batch;
}
