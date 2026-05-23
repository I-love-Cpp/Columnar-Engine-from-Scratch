#pragma once

#include "date_utils.h"

#include <algorithm>
#include <string>

bool IsLeap(const int32_t year) {
    return (year % 4 == 0 && year % 100 != 0) || year % 400 == 0;
}

int32_t ParseDateToDays(std::string_view date) {
    const int32_t y = (date[0] - '0') * 1000 + (date[1] - '0') * 100 + (date[2] - '0') * 10 + (date[3] - '0');
    const int32_t m = (date[5] - '0') * 10 + (date[6] - '0');
    const int32_t d = (date[8] - '0') * 10 + (date[9] - '0');
    int32_t leap = (y - 1) / 4 - (y - 1) / 100 + (y - 1) / 400;
    int32_t not_leap = y - leap - 1;
    int32_t days = leap * 366 + not_leap * 365 + d;
    if (m > 1) {
        days += DAYS_IN_MONTH[m - 2];
    }
    if (m > 2 && IsLeap(y)) {
        days++;
    }
    return days - 719163;
}

int32_t ParseDateToDays(int32_t y, int32_t m, int32_t d) {
    int32_t leap = (y - 1) / 4 - (y - 1) / 100 + (y - 1) / 400;
    int32_t not_leap = y - leap - 1;
    int32_t days = leap * 366 + not_leap * 365 + d;
    if (m > 1) {
        days += DAYS_IN_MONTH[m - 2];
    }
    if (m > 2 && IsLeap(y)) {
        days++;
    }
    return days - 719163;
}

std::string ParseDaysToDate(int32_t days) {
    int32_t res_year, res_month, res_day;

    int l = 1970, r = 3000;

    while (r - l > 1) {
        int32_t mid = (r + l) / 2;

        int32_t have_days = ParseDateToDays(mid, 1, 1);
        int32_t ost = days - have_days;

        if (ost >= 0) {
            l = mid;
        } else {
            r = mid;
        }
    }

    res_year = l;
    days -= ParseDateToDays(res_year, 1, 1);
    if (IsLeap(res_year)) {
        res_month = std::upper_bound(DAYS_IN_MONTH_IN_LEAP.begin(), DAYS_IN_MONTH_IN_LEAP.end(), days) -
                    DAYS_IN_MONTH_IN_LEAP.begin() + 1;
        if (res_month >= 2) {
            res_day = days - DAYS_IN_MONTH_IN_LEAP[res_month - 2] + 1;
        } else {
            res_day = days + 1;
        }
    } else {
        res_month = std::upper_bound(DAYS_IN_MONTH.begin(), DAYS_IN_MONTH.end(), days) - DAYS_IN_MONTH.begin() + 1;
        if (res_month >= 2) {
            res_day = days - DAYS_IN_MONTH[res_month - 2] + 1;
        } else {
            res_day = days + 1;
        }
    }

    std::string res = std::to_string(res_year) + "-";
    if (res_month <= 9) {
        res += "0" + std::to_string(res_month);
    } else {
        res += std::to_string(res_month);
    }
    res += "-";
    if (res_day <= 9) {
        res += "0" + std::to_string(res_day);
    } else {
        res += std::to_string(res_day);
    }

    return res;
}


int64_t ParseTimestampToMicros(std::string_view date) {
    std::string_view ymd = date.substr(0, 10);
    int64_t res = static_cast<int64_t>(ParseDateToDays(ymd)) * 86400LL * 1000000LL;
    if (date.size() > 10) {
        int64_t h = (date[11] - '0') * 10 + (date[12] - '0');
        int64_t m = (date[14] - '0') * 10 + (date[15] - '0');
        int64_t s = (date[17] - '0') * 10 + (date[18] - '0');
        res += (h * 3600 + m * 60 + s) * 1000000LL;
    }
    return res;
}

std::string ParseMicrosToTimestamp(int64_t micros) {
    int64_t sec = micros / 1000000;
    int64_t days = sec / 86400;
    sec %= 86400;
    int64_t hours = sec / 3600;
    sec %= 3600;
    int64_t mins = sec / 60;
    sec %= 60;
    std::string res = ParseDaysToDate(static_cast<int32_t>(days)) + " ";
    if (hours <= 9) {
        res += "0" + std::to_string(hours);
    } else {
        res += std::to_string(hours);
    }
    res += ":";
    if (mins <= 9) {
        res += "0" + std::to_string(mins);
    } else {
        res += std::to_string(mins);
    }
    res += ":";
    if (sec <= 9) {
        res += "0" + std::to_string(sec);
    } else {
        res += std::to_string(sec);
    }

    return res;
}
