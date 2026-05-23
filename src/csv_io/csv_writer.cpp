#include "csv_writer.h"
#include <fstream>
#include <stdexcept>

struct CsvWriter::Impl {
    std::ofstream file;

    void WriteRow(const std::vector<std::string> &fields) {
        for (size_t i = 0; i < fields.size(); i++) {
            if (i > 0) {
                file << ',';
            }

            bool need = false;
            for (char c: fields[i]) {
                if (c == ',' || c == '"' || c == '\n' || c == '\r') {
                    need = true;
                    break;
                }
            }

            if (need) {
                for (int j = 0; j < fields[i].size(); j++) {
                    char c = fields[i][j];
                    if (c == '"' && j != 0 && j != fields[i].size() - 1) {
                        file << "\"\"";
                    } else {
                        file << c;
                    }
                }
            } else {
                file << fields[i];
            }
        }
        file << '\n';
    }
};

CsvWriter::CsvWriter(const std::string &path) : impl_(std::make_unique<Impl>()) {
    impl_->file.open(path);
    if (!impl_->file) {
        throw std::runtime_error("Cannot create: " + path);
    }
}

CsvWriter::~CsvWriter() = default;

void CsvWriter::WriteRow(const std::vector<std::string> &fields) const {
    impl_->WriteRow(fields);
}

void CsvWriter::Flush() const {
    impl_->file.flush();
}
