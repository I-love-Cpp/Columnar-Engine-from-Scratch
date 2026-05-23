#pragma once

#include <unordered_map>

#include "types.h"

class TypeRegister {
public:
    static TypeRegister &instance();

    void RegisterType(std::shared_ptr<TypeImp> impl);

    const TypeImp &Get(TypesId id) const;

    const TypeImp &Get(std::string_view name) const;

    TypesId GetIdByName(std::string_view name) const;

private:
    TypeRegister() = default;

    std::unordered_map<TypesId, std::shared_ptr<TypeImp> > IdMap_;
    std::unordered_map<std::string, std::shared_ptr<TypeImp> > NameMap_;
};
