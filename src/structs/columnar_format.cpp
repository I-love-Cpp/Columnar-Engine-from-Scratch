#include "columnar_format.h"
#include "../types/type_register.h"
#include <fstream>
#include <cstring>
#include <numeric>
#include <stdexcept>

static const char MAGIC[4] = {'C', 'O', 'L', 'M'};
static const uint16_t VERSION = 1;

static void wr(std::ofstream &f, const void *d, size_t n) {
    f.write(static_cast<const char *>(d), n);
}

static void rd(std::ifstream &f, void *d, size_t n) {
    f.read(static_cast<char *>(d), n);
}

static uint16_t r16(std::ifstream &f) {
    uint16_t v;
    rd(f, &v, 2);
    return v;
}

static uint32_t r32(std::ifstream &f) {
    uint32_t v;
    rd(f, &v, 4);
    return v;
}

static uint64_t r64(std::ifstream &f) {
    uint64_t v;
    rd(f, &v, 8);
    return v;
}

struct ChunkInfo {
    uint64_t offset;
    uint64_t size;
};

struct RGInfo {
    uint32_t numRows{};
    std::vector<ChunkInfo> chunks;
};

struct ColumnarWriter::Impl {
    std::ofstream file;
    Schema schema;
    std::vector<RGInfo> rgInfos;
    uint64_t curOff = 0;

    void WriteHeader() {
        wr(file, MAGIC, 4);
        wr(file, &VERSION, 2);
        const auto nc = static_cast<uint16_t>(schema.NumColumns());
        wr(file, &nc, 2);
        constexpr uint32_t ph = 0;
        wr(file, &ph, 4);
        curOff = 12;
    }

    void WriteRG(const RowGroup &rg) {
        RGInfo info;
        info.numRows = static_cast<uint32_t>(rg.numRows);
        for (size_t c = 0; c < rg.columns.size(); c++) {
            ChunkInfo ci;
            ci.offset = curOff;
            auto &col = *rg.columns[c];
            if (col.Type().isFixedSize()) {
                auto *fc = dynamic_cast<const FixedColumn *>(&col);
                wr(file, fc->RawData(), fc->RawBytes());
                ci.size = fc->RawBytes();
            } else {
                auto *vc = dynamic_cast<const VarLenColumn *>(&col);
                auto n = static_cast<uint32_t>(vc->Size());
                wr(file, &n, 4);
                ci.size = 4;
                wr(file, vc->Offsets().data(), vc->Offsets().size() * 4);
                ci.size += vc->Offsets().size() * 4;
                wr(file, vc->Data().data(), vc->Data().size());
                ci.size += vc->Data().size();
            }
            curOff += ci.size;
            info.chunks.push_back(ci);
        }
        rgInfos.push_back(info);
    }

    void WriteFooter() {
        uint64_t footerOff = curOff;
        auto nc = static_cast<uint16_t>(schema.NumColumns());
        wr(file, &nc, 2);
        for (auto &cd: schema.columns) {
            auto nl = static_cast<uint16_t>(cd.name.size());
            wr(file, &nl, 2);
            wr(file, cd.name.data(), nl);
            auto tc = static_cast<uint8_t>(cd.typeId);
            wr(file, &tc, 1);
        }
        const auto nrg = static_cast<uint32_t>(rgInfos.size());
        wr(file, &nrg, 4);
        for (auto &ri: rgInfos) {
            wr(file, &ri.numRows, 4);
            for (auto &ci: ri.chunks) {
                wr(file, &ci.offset, 8);
                wr(file, &ci.size, 8);
            }
        }
        wr(file, &footerOff, 8);
        wr(file, MAGIC, 4);
        file.flush();
    }
};

ColumnarWriter::ColumnarWriter(const std::string &path, const Schema &schema)
    : impl_(std::make_unique<Impl>()) {
    impl_->file.open(path, std::ios::binary);
    if (!impl_->file) {
        throw std::runtime_error("Cannot create: " + path);
    }

    impl_->schema = schema;
    impl_->WriteHeader();
}

ColumnarWriter::~ColumnarWriter() = default;

void ColumnarWriter::WriteRowGroup(const RowGroup &rg) {
    impl_->WriteRG(rg);
}

void ColumnarWriter::Finish() {
    impl_->WriteFooter();
}

struct ColumnarReader::Impl {
    std::string path;
    Schema schema;
    std::vector<RGInfo> rgInfos;

    void Open() {
        std::ifstream f(path, std::ios::binary);
        if (!f) {
            throw std::runtime_error("Cannot open: " + path);
        }

        f.seekg(-12, std::ios::end);
        uint64_t footerOff = r64(f);
        char mag[4];
        rd(f, mag, 4);
        if (std::memcmp(mag, MAGIC, 4) != 0) {
            throw std::runtime_error("Bad magic");
        }

        f.seekg(0);
        rd(f, mag, 4);
        if (std::memcmp(mag, MAGIC, 4) != 0) {
            throw std::runtime_error("Bad magic");
        }

        f.seekg(static_cast<std::streamoff>(footerOff));
        const uint16_t nc = r16(f);
        for (uint16_t i = 0; i < nc; i++) {
            uint16_t nl = r16(f);
            std::string name(nl, '\0');
            rd(f, name.data(), nl);
            uint8_t tc;
            rd(f, &tc, 1);
            schema.columns.push_back({name, (TypesId) tc});
        }

        const uint32_t nrg = r32(f);
        for (uint32_t i = 0; i < nrg; i++) {
            RGInfo ri;
            ri.numRows = r32(f);
            for (uint16_t c = 0; c < nc; c++) {
                ChunkInfo ci{};
                ci.offset = r64(f);
                ci.size = r64(f);
                ri.chunks.push_back(ci);
            }
            rgInfos.push_back(ri);
        }
    }

    RowGroup ReadRG(size_t idx, const std::vector<size_t> &cols) const {
        auto &ri = rgInfos[idx];
        std::ifstream f(path, std::ios::binary);
        RowGroup rg;
        rg.numRows = ri.numRows;
        for (size_t ci: cols) {
            auto &chunk = ri.chunks[ci];
            auto &cd = schema.columns[ci];
            f.seekg((std::streamoff) chunk.offset);
            auto col = Column::Create(cd.typeId);
            if (col->Type().isFixedSize()) {
                auto *fc = dynamic_cast<FixedColumn *>(col.get());
                std::vector<uint8_t> buf(chunk.size);
                rd(f, buf.data(), buf.size());
                fc->LoadRaw(buf.data(), ri.numRows);
            } else {
                auto *vc = dynamic_cast<VarLenColumn *>(col.get());
                uint32_t n = r32(f);
                std::vector<uint32_t> offs(n + 1);
                rd(f, offs.data(), offs.size() * 4);
                uint32_t dataLen = offs.back();
                std::vector<uint8_t> data(dataLen);
                rd(f, data.data(), dataLen);
                vc->LoadRaw(offs, data);
            }
            rg.columns.push_back(ColumnPtr(col.release()));
        }
        return rg;
    }
};

ColumnarReader::ColumnarReader(const std::string &path) : impl_(std::make_unique<Impl>()) {
    impl_->path = path;
    impl_->Open();
}

ColumnarReader::~ColumnarReader() = default;

const Schema &ColumnarReader::GetSchema() const {
    return impl_->schema;
}

size_t ColumnarReader::NumRowGroups() const {
    return impl_->rgInfos.size();
}

RowGroup ColumnarReader::ReadRowGroup(size_t idx) const {
    std::vector<size_t> all(impl_->schema.NumColumns());
    std::iota(all.begin(), all.end(), 0);
    return impl_->ReadRG(idx, all);
}

RowGroup ColumnarReader::ReadRowGroupColumns(size_t idx, const std::vector<size_t> &cols) const {
    return impl_->ReadRG(idx, cols);
}
