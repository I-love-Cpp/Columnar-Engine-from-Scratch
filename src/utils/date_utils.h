#pragma once

#include <cstdint>
#include <string_view>
#include <vector>

const std::vector DAYS_IN_MONTH = {31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334, 365};

const std::vector DAYS_IN_MONTH_IN_LEAP = {31, 60, 91, 121, 152, 182, 213, 244, 274, 305, 335, 366};


bool IsLeap(int32_t year);

int32_t ParseDateToDays(std::string_view date);

int32_t ParseDateToDays(int32_t year, int32_t month, int32_t day);

std::string ParseDaysToDate(int32_t days);

int64_t ParseTimestampToMicros(std::string_view date);

std::string ParseMicrosToTimestamp(int64_t micros);
