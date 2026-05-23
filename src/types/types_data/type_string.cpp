#include <charconv>
#include <cstring>

#include "../types.h"
#include "../type_register.h"

class StringType : public TypeImp {
public:
    TypesId GetTypeId() const override {
        return TypesId::string;
    }

    const char *GetTypeName() const override {
        return "string";
    }

    size_t GetFixedSize() const override {
        return 0;
    }

    size_t Parse(std::string_view s, uint8_t *dst) const override {
        std::memcpy(dst, s.data(), s.size());
        return s.size();
    }

    std::string Format(const uint8_t *src, size_t len) const override {
        return std::string(reinterpret_cast<const char *>(src), len);
    }


    int Compare(const uint8_t *a, size_t aLen, const uint8_t *b, size_t bLen) const override {
        int r = std::memcmp(a, b, std::min(aLen, bLen));
        if (r != 0) {
            return r;
        }
        return (aLen > bLen) - (aLen < bLen);
    }
};


static struct RegisterString {
    RegisterString() {
        TypeRegister::instance().RegisterType(
            std::make_shared<StringType>());
    }
} g_registerString;
