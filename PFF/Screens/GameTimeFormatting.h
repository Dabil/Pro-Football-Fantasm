#pragma once

#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>
#include <cstdio>
#include <iomanip>
#include <locale>
#include <optional>
#include <sstream>
#include <string>

namespace PffGameTime
{
    inline std::optional<SYSTEMTIME> easternTime(const std::string& timestamp)
    {
        int year=0, month=0, day=0, hour=0, minute=0, second=0;
        if (sscanf_s(timestamp.c_str(), "%d-%d-%dT%d:%d:%d", &year, &month,
            &day, &hour, &minute, &second) != 6 || year < 1601 || year > 9999 ||
            month < 1 || month > 12 || day < 1 || day > 31 ||
            hour < 0 || hour > 23 || minute < 0 || minute > 59 || second < 0 || second > 59)
            return std::nullopt;

        SYSTEMTIME utc{};
        utc.wYear=static_cast<WORD>(year); utc.wMonth=static_cast<WORD>(month);
        utc.wDay=static_cast<WORD>(day); utc.wHour=static_cast<WORD>(hour);
        utc.wMinute=static_cast<WORD>(minute); utc.wSecond=static_cast<WORD>(second);
        static const auto zone = []() -> std::optional<DYNAMIC_TIME_ZONE_INFORMATION>
        {
            DYNAMIC_TIME_ZONE_INFORMATION candidate{};
            for (DWORD index=0; EnumDynamicTimeZoneInformation(index,&candidate)==ERROR_SUCCESS; ++index)
                if (std::wstring(candidate.TimeZoneKeyName)==L"Eastern Standard Time") return candidate;
            return std::nullopt;
        }();
        if (!zone) return std::nullopt;
        auto selectedZone = *zone;
        TIME_ZONE_INFORMATION historicalRules{};
        SYSTEMTIME local{};
        if (!GetTimeZoneInformationForYear(utc.wYear,&selectedZone,&historicalRules) ||
            !SystemTimeToTzSpecificLocalTime(&historicalRules,&utc,&local)) return std::nullopt;
        return local;
    }

    inline std::string clock(const SYSTEMTIME& local)
    {
        std::ostringstream text;
        text.imbue(std::locale::classic());
        text << (local.wHour%12==0 ? 12 : local.wHour%12) << ':'
            << std::setfill('0') << std::setw(2) << local.wMinute
            << (local.wHour<12 ? " AM ET" : " PM ET");
        return text.str();
    }

    inline std::string formatGameTimeEastern(const std::string& timestamp)
    {
        const auto local=easternTime(timestamp);
        if (!local) return "Game time unavailable";
        static const char* months[]={"January","February","March","April","May","June",
            "July","August","September","October","November","December"};
        return std::string(months[local->wMonth-1])+" "+std::to_string(local->wDay)+", "+
            std::to_string(local->wYear)+"  "+clock(*local);
    }

    inline std::string dayAndDate(const SYSTEMTIME& local)
    {
        static const char* days[]={"Sun","Mon","Tue","Wed","Thu","Fri","Sat"};
        std::ostringstream text;
        text.imbue(std::locale::classic());
        text << days[local.wDayOfWeek] << ' ' << std::setfill('0')
            << std::setw(2) << local.wMonth << '/' << std::setw(2) << local.wDay << '/' << local.wYear;
        return text.str();
    }
}
