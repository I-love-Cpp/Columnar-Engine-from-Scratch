#pragma once

#include <memory>
#include <string>
#include <vector>

class CsvWriter {
public:
    explicit CsvWriter(const std::string &path);

    ~CsvWriter();

    void WriteRow(const std::vector<std::string> &fields) const;

    void Flush() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
