#include <charconv>
#include <cstring>

#include "../types.h"
#include "../type_register.h"
#include "../../utils/date_utils.h"

class DateType : public TypeImp {
public:
    TypesId GetTypeId() const override {
        return TypesId::date;
    }

    const char *GetTypeName() const override {
        return "DATE";
    }

    size_t GetFixedSize() const override {
        return sizeof(int32_t);
    }

    size_t Parse(std::string_view s, uint8_t *dst) const override {
        int32_t d = ParseDateToDays(s);
        std::memcpy(dst, &d, 4);
        return sizeof(int32_t);
    }

    std::string Format(const uint8_t *src, size_t len) const override {
        int32_t d;
        std::memcpy(&d, src, 4);
        return ParseDaysToDate(d);
    }


    int Compare(const uint8_t *a, size_t, const uint8_t *b, size_t) const override {
        int32_t va, vb;
        std::memcpy(&va, a, 4);
        std::memcpy(&vb, b, 4);
        return (va > vb) - (va < vb);
    }
};


static struct RegisterDate {
    RegisterDate() {
        TypeRegister::instance().RegisterType(
            std::make_shared<DateType>());
    }
} g_registerDate;
