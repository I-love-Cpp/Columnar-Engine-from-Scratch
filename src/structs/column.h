#pragma once

#include "../types/types.h"
#include "../types/type_register.h"
#include <vector>
#include <memory>
#include <cstring>

class Column {
public:
    static std::unique_ptr<Column> Create(TypesId id);

    virtual ~Column() = default;

    virtual TypesId GetTypeId() const = 0;

    virtual const TypeImp &Type() const = 0;

    virtual size_t Size() const = 0;

    virtual void Reserve(size_t n) = 0;

    virtual void Clear() = 0;

    virtual void PushFromString(std::string_view s) = 0;

    virtual std::string GetAsString(size_t idx) const = 0;

    virtual int Compare(size_t a, size_t b) const = 0;
};

using ColumnPtr = std::shared_ptr<Column>;

class FixedColumn : public Column {
public:
    explicit FixedColumn(const TypeImp &type);

    TypesId GetTypeId() const override;

    const TypeImp &Type() const override;

    size_t Size() const override;

    void Reserve(size_t n) override;

    void Clear() override;

    void PushFromString(std::string_view s) override;

    std::string GetAsString(size_t idx) const override;

    int Compare(size_t a, size_t b) const override;

    const uint8_t *RawData() const;

    size_t RawBytes() const;

    size_t ValueSize() const;

    void LoadRaw(const uint8_t *src, size_t numRows);

private:
    const TypeImp &type_;
    size_t vs_;
    std::vector<uint8_t> data_;
};

class VarLenColumn : public Column {
public:
    explicit VarLenColumn(const TypeImp &type);

    TypesId GetTypeId() const override;

    const TypeImp &Type() const override;

    size_t Size() const override;

    void Reserve(size_t n) override;

    void Clear() override;

    void PushFromString(std::string_view s) override;

    std::string GetAsString(size_t idx) const override;

    int Compare(size_t a, size_t b) const override;

    const std::vector<uint32_t> &Offsets() const;

    const std::vector<uint8_t> &Data() const;

    void LoadRaw(const std::vector<uint32_t> &offs, const std::vector<uint8_t> &d);

private:
    const TypeImp &type_;
    std::vector<uint32_t> offsets_;
    std::vector<uint8_t> data_;
};
