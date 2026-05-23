#include "csv_reader.h"
#include <fstream>
#include <stdexcept>

struct CsvReader::Impl {
    std::ifstream file;
    std::string buf;
    bool eof = false;

    std::optional<std::vector<std::string> > NextRow() {
        if (eof) {
            return std::nullopt;
        }
        buf.clear();
        if (!std::getline(file, buf)) {
            eof = true;
            return std::nullopt;
        }
        while (!buf.empty() && buf.back() == '\r') {
            buf.pop_back();
        }
        if (buf.empty() && file.peek() == EOF) {
            eof = true;
            return std::nullopt;
        }
        return ParseFields();
    }

    std::vector<std::string> ParseFields() {
        std::vector<std::string> fields;
        size_t pos = 0;
        while (pos <= buf.size()) {
            if (pos < buf.size() && buf[pos] == '"') {
                fields.push_back(ParseQuoted(pos));
            } else {
                fields.push_back(ParseUnquoted(pos));
            }
        }
        return fields;
    }

    std::string ParseUnquoted(size_t &pos) const {
        std::string field;
        while (pos < buf.size() && buf[pos] != ',') {
            field += buf[pos++];
        }
        pos++;
        return field;
    }

    std::string ParseQuoted(size_t &pos) {
        pos++;
        std::string field;
        while (true) {
            while (pos < buf.size()) {
                char c = buf[pos++];
                if (c == '"') {
                    if (pos < buf.size() && buf[pos] == '"') {
                        field += '"';
                        pos++;
                    } else {
                        pos++;
                        return field;
                    }
                } else {
                    field += c;
                }
            }
            field += '\n';
            buf.clear();
            if (!std::getline(file, buf)) {
                eof = true;
                return field;
            }
            while (!buf.empty() && buf.back() == '\r') {
                buf.pop_back();
            }
            pos = 0;
        }
    }
};

CsvReader::CsvReader(const std::string &path) : impl_(std::make_unique<Impl>()) {
    impl_->file.open(path);
    if (!impl_->file) {
        throw std::runtime_error("Cannot open: " + path);
    }
}

CsvReader::~CsvReader() = default;

std::optional<std::vector<std::string> > CsvReader::NextRow() const {
    return impl_->NextRow();
}
