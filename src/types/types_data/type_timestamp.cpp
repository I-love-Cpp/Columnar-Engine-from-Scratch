#include <charconv>
#include <cstring>

#include "../types.h"
#include "../type_register.h"
#include "../../utils/date_utils.h"

class TimestampType : public TypeImp {
public:
    TypesId GetTypeId() const override {
        return TypesId::timestamp;
    }

    const char *GetTypeName() const override {
        return "TIMESTAMP";
    }

    size_t GetFixedSize() const override {
        return sizeof(int64_t);
    }

    size_t Parse(std::string_view s, uint8_t *dst) const override {
        int64_t d = ParseTimestampToMicros(s);
        std::memcpy(dst, &d, 8);
        return sizeof(int64_t);
    }

    std::string Format(const uint8_t *src, size_t len) const override {
        int64_t d;
        std::memcpy(&d, src, 8);
        return ParseMicrosToTimestamp(d);
    }


    int Compare(const uint8_t *a, size_t, const uint8_t *b, size_t) const override {
        int64_t va, vb;
        std::memcpy(&va, a, 8);
        std::memcpy(&vb, b, 8);
        return (va > vb) - (va < vb);
    }
};


static struct RegisterTimestamp {
    RegisterTimestamp() {
        TypeRegister::instance().RegisterType(
            std::make_shared<TimestampType>());
    }
} g_registerTimestamp;
