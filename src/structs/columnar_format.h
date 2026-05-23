#pragma once

#include "column.h"
#include "schema.h"
#include <memory>
#include <string>
#include <vector>

struct RowGroup {
    std::vector<ColumnPtr> columns;
    size_t numRows = 0;
};

class ColumnarWriter {
public:
    ColumnarWriter(const std::string &path, const Schema &schema);

    ~ColumnarWriter();

    void WriteRowGroup(const RowGroup &rg);

    void Finish();

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

class ColumnarReader {
public:
    explicit ColumnarReader(const std::string &path);

    ~ColumnarReader();

    const Schema &GetSchema() const;

    size_t NumRowGroups() const;

    RowGroup ReadRowGroup(size_t idx) const;

    RowGroup ReadRowGroupColumns(size_t idx, const std::vector<size_t> &cols) const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
