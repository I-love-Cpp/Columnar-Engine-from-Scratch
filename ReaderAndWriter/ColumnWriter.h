#pragma once
#include <fstream>
#include <vector>
#include <string>
#include <variant>
#include <cstdint>
#include "Types.h"

class ColumnWriter {
    size_t butch_limit_;

    std::ofstream out_;
    Schema schema_;
    FileMeta meta_;

    int64_t curr_rows_ = 0;
    std::vector<ColumnData> butch_;

public:
    ColumnWriter(const std::string &output_file_name, const std::string &schema_file_name,
                 const size_t butch_limit = 4);

    void AddRow(const std::vector<std::string> &row);

    void Close();

private:
    void NewButch();

    void ButchFlush();

    void WriteFooter();
};
