#include "schema.h"
#include "../types/type_register.h"
#include <fstream>
#include <stdexcept>

Schema Schema::LoadFromCSV(const std::string &path) {
    std::ifstream f(path);
    if (!f) {
        throw std::runtime_error("Cannot open schema: " + path);
    }

    Schema schema;
    std::string line;
    while (std::getline(f, line)) {
        while (!line.empty() && (line.back() == '\r' || line.back() == '\n')) {
            line.pop_back();
        }

        if (line.empty()) {
            continue;
        }

        auto comma = line.find(',');
        if (comma == std::string::npos) {
            throw std::runtime_error("Bad schema line: " + line);
        }

        std::string name = line.substr(0, comma);
        std::string typeName = line.substr(comma + 1);
        while (!typeName.empty() && typeName.front() == ' ') {
            typeName.erase(0, 1);
        }
        while (!typeName.empty() && (typeName.back() == ' ' || typeName.back() == ',')) {
            typeName.pop_back();
        }

        schema.columns.push_back({name, TypeRegister::instance().GetIdByName(typeName)});
    }
    return schema;
}

void Schema::SaveToCSV(const std::string &path) const {
    std::ofstream f(path);
    for (auto &col: columns) {
        f << col.name << "," << TypeRegister::instance().Get(col.typeId).GetTypeName() << "\n";
    }
}
