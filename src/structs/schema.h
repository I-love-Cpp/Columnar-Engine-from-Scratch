#pragma once

#include "../types/types.h"
#include <string>
#include <vector>

struct ColumnDef {
    std::string name;
    TypesId typeId;
};

struct Schema {
    std::vector<ColumnDef> columns;
    size_t NumColumns() const { return columns.size(); }

    int FindColumn(const std::string &name) const {
        for (size_t i = 0; i < columns.size(); i++)
            if (columns[i].name == name) return static_cast<int>(i);
        return -1;
    }

    static Schema LoadFromCSV(const std::string &path);

    void SaveToCSV(const std::string &path) const;
};
