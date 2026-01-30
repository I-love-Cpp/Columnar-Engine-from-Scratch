#include "ColumnWriter.h"
#include "Utils.h"

ColumnWriter::ColumnWriter(const std::string &output_file_name, const std::string &schema_file_name,
                           const size_t butch_limit) : butch_limit_(
    butch_limit) {
    schema_ = ReadSchema(schema_file_name);
    out_.open(output_file_name, std::ios::binary);
    if (!out_.is_open()) {
        throw std::runtime_error("Cant open output file");
    }

    out_.write(MAGIC, 4);
    meta_.schema = schema_;
    NewButch();
}

void ColumnWriter::AddRow(const std::vector<std::string> &row) {
    if (row.size() != schema_.size()) {
        throw std::runtime_error("Wrong row");
    }

    for (int i = 0; i < static_cast<int32_t>(schema_.size()); i++) {
        if (schema_[i].type == DataType::INT64) {
            int64_t val = row[i].empty() ? 0 : std::stoll(row[i]);
            std::get<std::vector<int64_t> >(butch_[i]).push_back(val);
        } else {
            std::get<std::vector<std::string> >(butch_[i]).push_back(row[i]);
        }
    }
    curr_rows_++;
    meta_.rows++;

    if (curr_rows_ == butch_limit_) {
        ButchFlush();
    }
}

void ColumnWriter::Close() {
    if (curr_rows_ > 0) {
        ButchFlush();
    }
    WriteFooter();
    out_.close();
}

void ColumnWriter::NewButch() {
    curr_rows_ = 0;
    butch_.clear();
    for (const auto &[name, type]: schema_) {
        if (type == DataType::INT64) {
            butch_.emplace_back(std::vector<int64_t>());
        } else {
            butch_.emplace_back(std::vector<std::string>());
        }
    }
}

void ColumnWriter::ButchFlush() {
    ChunkMeta curr_chunk_meta;
    curr_chunk_meta.rows = curr_rows_;
    for (int i = 0; i < static_cast<int32_t>(schema_.size()); i++) {
        uint64_t offset = out_.tellp();
        curr_chunk_meta.offsets.push_back(offset);

        if (schema_[i].type == DataType::INT64) {
            const auto &vec = std::get<std::vector<int64_t> >(butch_[i]);
            out_.write(reinterpret_cast<const char *>(vec.data()), vec.size() * sizeof(int64_t));
        } else {
            const auto &vec = std::get<std::vector<std::string> >(butch_[i]);
            for (const auto &s: vec) {
                auto len = static_cast<uint32_t>(s.size());
                out_.write(reinterpret_cast<const char *>(&len), sizeof(len));
                out_.write(s.data(), len);
            }
        }
    }

    meta_.chunks.push_back(curr_chunk_meta);
    NewButch();
}

void ColumnWriter::WriteFooter() {
    const uint64_t meta_start_offset = out_.tellp();

    uint32_t col_count = schema_.size();
    out_.write(reinterpret_cast<char *>(&col_count), sizeof(col_count));
    for (const auto &[name, type]: schema_) {
        uint32_t n_len = name.size();
        out_.write(reinterpret_cast<char *>(&n_len), sizeof(n_len));
        out_.write(name.data(), n_len);
        uint8_t t = (type == DataType::INT64) ? 0 : 1;
        out_.write(reinterpret_cast<char *>(&t), sizeof(t));
    }

    out_.write(reinterpret_cast<char *>(&meta_.rows), sizeof(meta_.rows));

    uint32_t chunk_count = meta_.chunks.size();
    out_.write(reinterpret_cast<char *>(&chunk_count), sizeof(chunk_count));
    for (const auto &[rows, offsets]: meta_.chunks) {
        out_.write((char *) &rows, sizeof(rows));
        for (uint64_t off: offsets) {
            out_.write(reinterpret_cast<char *>(&off), sizeof(off));
        }
    }

    const uint64_t meta_end_offset = out_.tellp();
    const auto meta_len = static_cast<uint32_t>(meta_end_offset - meta_start_offset);

    out_.write(reinterpret_cast<const char *>(&meta_len), sizeof(meta_len));
    out_.write(MAGIC, 4);
}
