#pragma once

#include <memory>
#include <optional>
#include <string>
#include <vector>

class CsvReader {
public:
    explicit CsvReader(const std::string &path);

    ~CsvReader();

    std::optional<std::vector<std::string> > NextRow() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
