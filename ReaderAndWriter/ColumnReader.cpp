#include "ColumnReader.h"
#include "Utils.h"

ColumnarReader::ColumnarReader(const std::string &filename) {
    in_.open(filename, std::ios::binary);
    if (!in_.is_open()) {
        throw std::runtime_error("Cant open file");
    }
    ReadFooter();
}

void ColumnarReader::ToCSV(const std::string &out_csv_name) {
    std::ofstream csv(out_csv_name);

    for (const auto &[rows, offsets]: meta_.chunks) {
        std::vector<ColumnData> chunk_data;
        for (size_t i = 0; i < meta_.schema.size(); i++) {
            in_.seekg(offsets[i]);
            if (DataType type = meta_.schema[i].type; type == DataType::INT64) {
                std::vector<int64_t> vec(rows);
                in_.read(reinterpret_cast<char *>(vec.data()), rows * sizeof(int64_t));
                chunk_data.emplace_back(std::move(vec));
            } else {
                std::vector<std::string> vec;
                vec.reserve(rows);
                for (size_t j = 0; j < rows; j++) {
                    uint32_t len;
                    in_.read(reinterpret_cast<char *>(&len), sizeof(len));
                    std::string s(len, '\0');
                    in_.read(&s[0], len);
                    vec.push_back(std::move(s));
                }
                chunk_data.emplace_back(std::move(vec));
            }
        }
        for (size_t j = 0; j < rows; j++) {
            for (size_t c = 0; c < meta_.schema.size(); c++) {
                if (c > 0) {
                    csv << ",";
                }
                if (meta_.schema[c].type == DataType::INT64) {
                    csv << std::get<std::vector<int64_t> >(chunk_data[c])[j];
                } else {
                    csv << StringToCSV(std::get<std::vector<std::string> >(chunk_data[c])[j]);
                }
            }
            csv << "\n";
        }
    }
}

void ColumnarReader::ReadFooter() {
    in_.seekg(-8, std::ios::end);
    const uint64_t file_len = in_.tellg() + static_cast<std::streamoff>(8);
    uint32_t meta_len;
    in_.read(reinterpret_cast<char *>(&meta_len), sizeof(meta_len));

    char magic[5] = {};
    in_.read(magic, 4);
    if (std::string(magic) != "PAR1") {
        throw std::runtime_error("Invalid footer magic");
    }

    in_.seekg(file_len - 8 - meta_len, std::ios::beg);
    uint32_t col_count;
    in_.read(reinterpret_cast<char *>(&col_count), sizeof(col_count));
    for (uint32_t i = 0; i < col_count; i++) {
        uint32_t n_len;
        in_.read(reinterpret_cast<char *>(&n_len), sizeof(n_len));
        std::string name(n_len, '\0');
        in_.read(&name[0], n_len);
        uint8_t t;
        in_.read(reinterpret_cast<char *>(&t), sizeof(t));
        DataType type = t == 0 ? DataType::INT64 : DataType::STRING;
        meta_.schema.emplace_back(name, type);
    }

    in_.read(reinterpret_cast<char *>(&meta_.rows), sizeof(meta_.rows));
    uint32_t chunk_count;
    in_.read(reinterpret_cast<char *>(&chunk_count), sizeof(chunk_count));
    for (uint32_t i = 0; i < chunk_count; i++) {
        ChunkMeta cm;
        in_.read(reinterpret_cast<char *>(&cm.rows), sizeof(cm.rows));
        cm.offsets.resize(meta_.schema.size());
        for (size_t j = 0; j < meta_.schema.size(); j++) {
            in_.read(reinterpret_cast<char *>(&cm.offsets[j]), sizeof(uint64_t));
        }
        meta_.chunks.push_back(cm);
    }
}
