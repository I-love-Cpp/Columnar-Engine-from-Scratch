#include "global_aggregate.h"
#include "../structs/column.h"

GlobalAggregate::GlobalAggregate(std::unique_ptr<IOperator> upstream, std::vector<Spec> specs)
    : upstream_(std::move(upstream)), specs_(std::move(specs)) {
    states_.resize(specs_.size());
}

std::optional<Batch> GlobalAggregate::Next() {
    if (done_) {
        return std::nullopt;
    }

    done_ = true;
    while (auto batch_opt = upstream_->Next()) {
        Batch &batch = *batch_opt;
        const size_t rows = batch.ActiveRows();
        batch.HasSelection() ? batch.selection : std::vector<uint32_t>{};
        for (size_t r = 0; r < rows; ++r) {
            for (size_t s = 0; s < specs_.size(); ++s) {
                const auto &spec = specs_[s];
                AggState &st = states_[s];
                if (spec.op == AggOp::COUNT) {
                    st.count++;
                    continue;
                }
                const bool is_int = spec.arg->GetResultType() == ExprResultType::INT64;
                std::string str_val;
                int64_t int_val = 0;
                if (is_int) {
                    int_val = spec.arg->EvaluateAsInt64(batch, r);
                } else {
                    str_val = spec.arg->EvaluateAsString(batch, r);
                }

                switch (spec.op) {
                    case AggOp::SUM:
                        s += static_cast<__int128>(int_val);
                        st.count = 1;
                        break;
                    case AggOp::AVG:
                        s += static_cast<__int128>(int_val);
                        st.count++;
                        break;
                    case AggOp::MIN:
                        if (!st.has_minmax) {
                            st.has_minmax = true;
                            if (is_int) {
                                st.min_val = int_val;
                            } else {
                                st.min_str = str_val;
                            }
                        } else {
                            if (is_int) {
                                if (int_val < st.min_val) {
                                    st.min_val = int_val;
                                }
                            } else {
                                if (str_val < st.min_str) {
                                    st.min_str = str_val;
                                }
                            }
                        }
                        break;
                    case AggOp::MAX:
                        if (!st.has_minmax) {
                            st.has_minmax = true;
                            if (is_int) {
                                st.max_val = int_val;
                            } else {
                                st.max_str = str_val;
                            }
                        } else {
                            if (is_int) {
                                if (int_val > st.max_val) {
                                    st.max_val = int_val;
                                }
                            } else {
                                if (str_val > st.max_str) {
                                    st.max_str = str_val;
                                }
                            }
                        }
                        break;
                    case AggOp::COUNT_DISTINCT:
                        st.distinct_set.insert(is_int ? std::to_string(int_val) : str_val);
                        break;
                    default: break;
                }
            }
        }
    }

    result_.numRows = 1;
    result_.schema.columns.clear();
    result_.columns.clear();
    for (size_t s = 0; s < specs_.size(); ++s) {
        const auto &spec = specs_[s];
        const AggState &st = states_[s];
        auto tid = TypesId::string;
        std::string val_str;
        switch (spec.op) {
            case AggOp::COUNT:
                tid = TypesId::int64;
                val_str = std::to_string(st.count);
                break;
            case AggOp::SUM:
                tid = TypesId::int64;
                val_str = int128_to_string(s);
                break;
            case AggOp::AVG: {
                double avg = st.count ? static_cast<double>(s) / st.count : 0.0;
                val_str = std::to_string(static_cast<int64_t>(avg));
                tid = TypesId::int64;
                break;
            }
            case AggOp::MIN:
                if (spec.arg->GetResultType() == ExprResultType::INT64) {
                    tid = TypesId::int64;
                    val_str = std::to_string(st.has_minmax ? st.min_val : 0);
                } else {
                    val_str = st.has_minmax ? st.min_str : "";
                }
                break;
            case AggOp::MAX:
                if (spec.arg->GetResultType() == ExprResultType::INT64) {
                    tid = TypesId::int64;
                    val_str = std::to_string(st.has_minmax ? st.max_val : 0);
                } else {
                    val_str = st.has_minmax ? st.max_str : "";
                }
                break;
            case AggOp::COUNT_DISTINCT:
                tid = TypesId::int64;
                val_str = std::to_string(st.distinct_set.size());
                break;
        }

        result_.schema.columns.push_back({"agg" + std::to_string(s), tid});
        auto col = Column::Create(tid);
        col->PushFromString(val_str);
        result_.columns.push_back(std::shared_ptr<Column>(col.release()));
    }
    return result_;
}
