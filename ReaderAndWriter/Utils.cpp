#include "Utils.h"
#include <fstream>
#include <stdexcept>

std::vector<std::string> SplitCSV(const std::string &s, const char delim) {
    std::vector<std::string> cols;
    bool in_quotes = false;
    std::string curr;
    for (int i = 0; i < static_cast<int32_t>(s.size()); i++) {
        if (in_quotes) {
            if (s[i] == '"') {
                if (i + 1 < static_cast<int32_t>(s.size()) && s[i + 1] == '"') {
                    curr += '"';
                    i++;
                } else {
                    in_quotes = false;
                }
            } else {
                curr += s[i];
            }
        } else {
            if (s[i] == '"') {
                in_quotes = true;
            } else if (s[i] == delim) {
                cols.push_back(curr);
                curr.clear();
            } else {
                curr += s[i];
            }
        }
    }
    cols.push_back(curr);
    return cols;
}


Schema ReadSchema(const std::string &filename) {
    Schema schema;
    std::ifstream in_(filename);
    if (!in_.is_open()) {
        throw std::runtime_error("Cant open schema");
    }
    std::string s;
    while (std::getline(in_, s)) {
        std::vector<std::string> line = SplitCSV(s, ',');
        if (line.size() != 2) {
            throw std::runtime_error("Wrong schema");
        }
        DataType type = line[1] == "int64" ? DataType::INT64 : DataType::STRING;
        schema.emplace_back(line[0], type);
    }
    return schema;
}

std::string StringToCSV(const std::string &val) {
    if (val.find(',') == std::string::npos && val.find('"') == std::string::npos) {
        return val;
    }
    std::string res = "\"";
    for (const char el: val) {
        if (el == '"') {
            res += "\"\"";
        } else {
            res += el;
        }
    }
    res += "\"";
    return res;
}
