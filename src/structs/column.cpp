#include "column.h"

std::unique_ptr<Column> Column::Create(TypesId id) {
    const TypeImp &t = TypeRegister::instance().Get(id);
    if (t.isFixedSize()) {
        return std::make_unique<FixedColumn>(t);
    }

    return std::make_unique<VarLenColumn>(t);
}

FixedColumn::FixedColumn(const TypeImp &type) : type_(type), vs_(type.GetFixedSize()) {
}

TypesId FixedColumn::GetTypeId() const {
    return type_.GetTypeId();
}

const TypeImp &FixedColumn::Type() const {
    return type_;
}

size_t FixedColumn::Size() const {
    return data_.size() / vs_;
}

void FixedColumn::Reserve(size_t n) {
    data_.reserve(n * vs_);
}

void FixedColumn::Clear() {
    data_.clear();
}

size_t FixedColumn::ValueSize() const {
    return vs_;
}

const uint8_t *FixedColumn::RawData() const {
    return data_.data();
}

size_t FixedColumn::RawBytes() const {
    return data_.size();
}

void FixedColumn::PushFromString(std::string_view s) {
    const size_t off = data_.size();
    data_.resize(off + vs_);
    type_.Parse(s, data_.data() + off);
}

std::string FixedColumn::GetAsString(size_t idx) const {
    return type_.Format(data_.data() + idx * vs_, vs_);
}

int FixedColumn::Compare(size_t a, size_t b) const {
    return type_.Compare(data_.data() + a * vs_, vs_,
                         data_.data() + b * vs_, vs_);
}

void FixedColumn::LoadRaw(const uint8_t *src, size_t numRows) {
    data_.assign(src, src + numRows * vs_);
}

VarLenColumn::VarLenColumn(const TypeImp &type) : type_(type) {
    offsets_.push_back(0);
}

TypesId VarLenColumn::GetTypeId() const {
    return type_.GetTypeId();
}

const TypeImp &VarLenColumn::Type() const {
    return type_;
}

size_t VarLenColumn::Size() const {
    return offsets_.size() - 1;
}

void VarLenColumn::Reserve(size_t n) {
    offsets_.reserve(n + 1);
}

void VarLenColumn::Clear() {
    data_.clear();
    offsets_.clear();
    offsets_.push_back(0);
}

void VarLenColumn::PushFromString(std::string_view s) {
    data_.insert(data_.end(), s.begin(), s.end());
    offsets_.push_back(static_cast<uint32_t>(data_.size()));
}

std::string VarLenColumn::GetAsString(size_t idx) const {
    return type_.Format(data_.data() + offsets_[idx],
                        offsets_[idx + 1] - offsets_[idx]);
}

int VarLenColumn::Compare(size_t a, size_t b) const {
    return type_.Compare(
        data_.data() + offsets_[a], offsets_[a + 1] - offsets_[a],
        data_.data() + offsets_[b], offsets_[b + 1] - offsets_[b]);
}

const std::vector<uint32_t> &VarLenColumn::Offsets() const {
    return offsets_;
}

const std::vector<uint8_t> &VarLenColumn::Data() const {
    return data_;
}

void VarLenColumn::LoadRaw(const std::vector<uint32_t> &offs,
                           const std::vector<uint8_t> &d) {
    offsets_ = offs;
    data_ = d;
}
