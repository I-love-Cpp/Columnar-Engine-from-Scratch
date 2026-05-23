#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <memory>

enum class TypesId: uint8_t {
    int16 = 1,
    int32 = 2,
    int64 = 3,
    string = 4,
    date = 5,
    timestamp = 6,
};

class TypeImp {
public:
    virtual ~TypeImp() = default;

    virtual TypesId GetTypeId() const = 0;

    virtual const char *GetTypeName() const = 0;

    virtual size_t GetFixedSize() const = 0;

    virtual size_t Parse(std::string_view s, uint8_t *dst) const = 0;

    virtual std::string Format(const uint8_t *src, size_t len) const = 0;

    virtual int Compare(const uint8_t *a, size_t aLen,
                        const uint8_t *b, size_t bLen) const = 0;

    bool isFixedSize() const { return GetFixedSize() > 0; }
};
