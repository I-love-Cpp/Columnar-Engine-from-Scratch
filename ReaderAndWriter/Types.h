#pragma once

#include <vector>
#include <string>
#include <variant>
#include <cstdint>

constexpr char MAGIC[] = "PAR1";

enum class DataType {
    INT64 = 0,
    STRING = 1
};

struct ColumnDef {
    std::string name;
    DataType type;

    ColumnDef(const std::string &name_, const DataType type_) {
        name = name_;
        type = type_;
    }
};

struct ChunkMeta {
    int64_t rows = 0;
    std::vector<uint64_t> offsets;
};

using Schema = std::vector<ColumnDef>;
using ColumnData = std::variant<std::vector<int64_t>, std::vector<std::string> >;

struct FileMeta {
    Schema schema;
    std::vector<ChunkMeta> chunks;
    uint64_t rows = 0;
};
