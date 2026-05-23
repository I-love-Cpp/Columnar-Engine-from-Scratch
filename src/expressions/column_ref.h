#pragma once
#include "expression.h"
#include "../structs/column.h"
#include "../types/types.h"

class ColumnRef : public Expression {
    int index_;
    TypesId type_id_;

public:
    ColumnRef(const int index, const TypesId type_id) : index_(index), type_id_(type_id) {
    }

    explicit ColumnRef(const int index) : index_(index), type_id_(TypesId::string) {
    }

    bool EvaluateAsBool(const Batch &batch, const size_t row) const override {
        if (type_id_ == TypesId::int16 || type_id_ == TypesId::int32 || type_id_ == TypesId::int64) {
            return EvaluateAsInt64(batch, row) != 0;
        }
        return EvaluateAsString(batch, row) == "true";
    }

    ExprResultType GetResultType() const override {
        if (type_id_ == TypesId::int16 || type_id_ == TypesId::int32 || type_id_ == TypesId::int64) {
            return ExprResultType::INT64;
        }
        return ExprResultType::STRING;
    }

    std::string EvaluateAsString(const Batch &batch, const size_t row) const override {
        return batch.GetValue(index_, row);
    }

    int64_t EvaluateAsInt64(const Batch &batch, const size_t row) const override {
        const auto &col = *batch.columns[index_];
        if (col.Type().isFixedSize()) {
            const auto *fc = dynamic_cast<const FixedColumn *>(&col);
            size_t phys = row;
            if (batch.HasSelection()) {
                phys = batch.selection[row];
            }

            const uint8_t *raw = fc->RawData() + phys * fc->ValueSize();
            switch (type_id_) {
                case TypesId::int16: {
                    int16_t val;
                    std::memcpy(&val, raw, sizeof(val));
                    return val;
                }
                case TypesId::int32: {
                    int32_t val;
                    std::memcpy(&val, raw, sizeof(val));
                    return val;
                }
                case TypesId::int64: {
                    int64_t val;
                    std::memcpy(&val, raw, sizeof(val));
                    return val;
                }
                case TypesId::date: {
                    int32_t val;
                    std::memcpy(&val, raw, sizeof(val));
                    return val;
                }
                case TypesId::timestamp: {
                    int64_t val;
                    std::memcpy(&val, raw, sizeof(val));
                    return val;
                }
                default: break;
            }
        }
        return std::stoll(EvaluateAsString(batch, row));
    }
};
