#include "projection.h"
#include "../structs/column.h"

Projection::Projection(std::unique_ptr<IOperator> upstream,
                       std::vector<std::unique_ptr<Expression> > exprs,
                       std::vector<std::string> aliases)
    : upstream_(std::move(upstream)), exprs_(std::move(exprs)), aliases_(std::move(aliases)) {
}

std::optional<Batch> Projection::Next() {
    auto batch_opt = upstream_->Next();
    if (!batch_opt) {
        return std::nullopt;
    }
    const Batch &in = *batch_opt;
    Batch out;
    for (size_t i = 0; i < exprs_.size(); ++i) {
        const TypesId tid = (exprs_[i]->GetResultType() == ExprResultType::INT64) ? TypesId::int64 : TypesId::string;
        out.schema.columns.push_back({aliases_[i], tid});
        auto col = Column::Create(tid);
        const size_t rows = in.ActiveRows();
        in.HasSelection() ? in.selection : std::vector<uint32_t>{};
        for (size_t r = 0; r < rows; ++r) {
            if (tid == TypesId::int64) {
                const int64_t val = exprs_[i]->EvaluateAsInt64(in, r);
                col->PushFromString(std::to_string(val));
            } else {
                std::string val = exprs_[i]->EvaluateAsString(in, r);
                col->PushFromString(val);
            }
        }

        out.columns.push_back(std::shared_ptr<Column>(col.release()));
    }

    out.numRows = out.columns.empty() ? 0 : out.columns[0]->Size();
    return out;
}
