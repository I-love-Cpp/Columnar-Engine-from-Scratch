#include "type_register.h"

TypeRegister &TypeRegister::instance() {
    static TypeRegister type_register;
    return type_register;
}

void TypeRegister::RegisterType(std::shared_ptr<TypeImp> impl) {
    IdMap_[impl->GetTypeId()] = impl;
    NameMap_[impl->GetTypeName()] = impl;
}

const TypeImp &TypeRegister::Get(TypesId id) const {
    const auto it = IdMap_.find(id);
    if (it == IdMap_.end()) {
        throw std::runtime_error("Type not registered: " + std::to_string(static_cast<int>(id)));
    }
    return *it->second;
}

const TypeImp &TypeRegister::Get(std::string_view name) const {
    const auto it = NameMap_.find(std::string(name));
    if (it == NameMap_.end()) {
        throw std::runtime_error("Type not registered: " + std::string(name));
    }
    return *it->second;
}

TypesId TypeRegister::GetIdByName(std::string_view name) const {
    return Get(name).GetTypeId();
}
