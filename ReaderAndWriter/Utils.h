#pragma once

#include <vector>
#include <string>
#include "Types.h"

std::vector<std::string> SplitCSV(const std::string &s, char delim = ',');

std::string StringToCSV(const std::string &val);

Schema ReadSchema(const std::string &filename);
