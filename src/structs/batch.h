#pragma once

#include "column.h"
#include "schema.h"
#include <vector>

struct Batch {
    Schema schema;
    std::vector<ColumnPtr> columns;
    size_t numRows = 0;
    std::vector<uint32_t> selection;

    bool HasSelection() const { return !selection.empty(); }
    size_t ActiveRows() const { return HasSelection() ? selection.size() : numRows; }

    std::string GetValue(size_t col, size_t logicalRow) const {
        size_t phys = HasSelection() ? selection[logicalRow] : logicalRow;
        return columns[col]->GetAsString(phys);
    }

    int FindColumn(const std::string &name) const {
        return schema.FindColumn(name);
    }

    Batch Materialize() const {
        if (!HasSelection()) return *this;
        Batch r;
        r.schema = schema;
        r.numRows = selection.size();
        for (size_t c = 0; c < columns.size(); c++) {
            auto col = Column::Create(schema.columns[c].typeId);
            col->Reserve(selection.size());
            for (uint32_t idx: selection)
                col->PushFromString(columns[c]->GetAsString(idx));
            r.columns.push_back(std::shared_ptr<Column>(col.release()));
        }
        return r;
    }
};
