#include "group_aggregate.h"
#include "../structs/column.h"
#include <cstdio>

GroupAggregate::GroupAggregate(std::unique_ptr<IOperator> upstream,
                               std::vector<std::unique_ptr<Expression> > key_exprs,
                               std::vector<Spec> agg_specs)
    : upstream_(std::move(upstream)),
      key_exprs_(std::move(key_exprs)),
      agg_specs_(std::move(agg_specs)) {
}

std::optional<Batch> GroupAggregate::Next() {
    if (done_) {
        if (result_idx_ < result_batches_.size())
            return std::move(result_batches_[result_idx_++]);
        return std::nullopt;
    }

    done_ = true;
    auto get_selection = [](const Batch &batch) -> const std::vector<uint32_t> & {
        static const std::vector<uint32_t> empty;
        return batch.HasSelection() ? batch.selection : empty;
    };

    while (auto batch_opt = upstream_->Next()) {
        Batch &batch = *batch_opt;
        size_t rows = batch.ActiveRows();
        if (rows == 0) {
            continue;
        }

        const auto &sel = get_selection(batch);
        for (size_t r = 0; r < rows; ++r) {
            std::string key;
            for (size_t k = 0; k < key_exprs_.size(); ++k) {
                if (k > 0) {
                    key += '\0';
                }
                key += key_exprs_[k]->EvaluateAsString(batch, r);
            }

            auto &states = groups_[key];
            if (states.empty()) {
                states.resize(agg_specs_.size());
            }

            for (size_t s = 0; s < agg_specs_.size(); ++s) {
                const auto &spec = agg_specs_[s];
                AggState &st = states[s];
                if (spec.op == AggOp::COUNT) {
                    st.count++;
                    continue;
                }

                bool is_int = spec.arg->GetResultType() == ExprResultType::INT64;
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

    Batch out;
    for (size_t k = 0; k < key_exprs_.size(); ++k) {
        TypesId tid = (key_exprs_[k]->GetResultType() == ExprResultType::INT64)
                          ? TypesId::int64
                          : TypesId::string;
        out.schema.columns.push_back({"key" + std::to_string(k), tid});
        out.columns.push_back(std::shared_ptr(Column::Create(tid)));
    }
    for (size_t s = 0; s < agg_specs_.size(); ++s) {
        std::string name = "agg" + std::to_string(s);
        auto tid = TypesId::string;

        switch (agg_specs_[s].op) {
            case AggOp::COUNT:
            case AggOp::SUM:
            case AggOp::COUNT_DISTINCT:
                tid = TypesId::int64;
                break;
            case AggOp::AVG:
                tid = TypesId::int64;
                break;
            case AggOp::MIN:
            case AggOp::MAX:
                tid = TypesId::string;
                break;
        }
        out.schema.columns.push_back({name, tid});
        out.columns.push_back(std::shared_ptr<Column>(Column::Create(tid)));
    }

    size_t num_keys = key_exprs_.size();
    for (auto &[key_str, vec_states]: groups_) {
        size_t pos = 0;
        for (size_t k = 0; k < num_keys; ++k) {
            size_t end = key_str.find('\0', pos);
            std::string part = (end == std::string::npos) ? key_str.substr(pos) : key_str.substr(pos, end - pos);
            out.columns[k]->PushFromString(part);
            pos = (end == std::string::npos) ? key_str.size() : end + 1;
        }

        for (size_t s = 0; s < agg_specs_.size(); ++s) {
            const AggState &st = vec_states[s];
            std::string val_str;
            switch (agg_specs_[s].op) {
                case AggOp::COUNT: val_str = std::to_string(st.count);
                    break;
                case AggOp::SUM: val_str = int128_to_string(st.sum);
                    break;
                case AggOp::AVG: {
                    double avg = st.count ? static_cast<double>(st.sum) / st.count : 0.0;
                    val_str = std::to_string(static_cast<int64_t>(avg));
                    break;
                }
                case AggOp::MIN:
                    val_str = (agg_specs_[s].arg && agg_specs_[s].arg->GetResultType() == ExprResultType::INT64)
                                  ? std::to_string(st.min_val)
                                  : st.min_str;
                    break;
                case AggOp::MAX:
                    val_str = (agg_specs_[s].arg && agg_specs_[s].arg->GetResultType() == ExprResultType::INT64)
                                  ? std::to_string(st.max_val)
                                  : st.max_str;
                    break;
                case AggOp::COUNT_DISTINCT:
                    val_str = std::to_string(st.distinct_set.size());
                    break;
            }
            out.columns[num_keys + s]->PushFromString(val_str);
        }
    }

    out.numRows = groups_.size();
    result_batches_.push_back(std::move(out));
    result_idx_ = 1;
    return std::move(result_batches_[0]);
}
