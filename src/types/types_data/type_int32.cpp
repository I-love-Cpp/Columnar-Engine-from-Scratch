#include <charconv>
#include <cstring>

#include "../types.h"
#include "../type_register.h"

class Int32Type : public TypeImp {
public:
    TypesId GetTypeId() const override {
        return TypesId::int32;
    }

    const char  *GetTypeName() const override {
        return "int32";
    }

    size_t GetFixedSize() const override {
        return sizeof(int32_t);
    }

    size_t Parse(std::string_view s, uint8_t *dst) const override {
        int32_t val;
        if (!s.empty()) {
            std::from_chars(s.data(), s.data() + s.size(), val);
        }
        std::memcpy(dst, &val, sizeof(int32_t));
        return sizeof(int32_t);
    }

    std::string Format(const uint8_t *src, size_t len) const override {
        int32_t val;
        std::memcpy(&val, src, sizeof(int32_t));
        return std::to_string(val);
    }


    int Compare(const uint8_t *a, size_t,
                const uint8_t *b, size_t) const override {
        int32_t va, vb;
        std::memcpy(&va, a, sizeof(int32_t));
        std::memcpy(&vb, b, sizeof(int32_t));
        if (va < vb) return -1;
        if (va > vb) return 1;
        return 0;
    }
};


static struct RegisterInt32 {
    RegisterInt32() {
        TypeRegister::instance().RegisterType(
            std::make_shared<Int32Type>());
    }
} g_registerInt32;
