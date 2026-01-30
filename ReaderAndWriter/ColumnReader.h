#pragma once
#include <fstream>
#include <vector>
#include <string>
#include <variant>
#include <cstdint>
#include "Types.h"

class ColumnarReader {
    std::ifstream in_;
    FileMeta meta_;

public:
    explicit ColumnarReader(const std::string &filename);

    void ToCSV(const std::string &out_csv_name);

private:
    void ReadFooter();
};
