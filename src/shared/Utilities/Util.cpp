/**
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * MaNGOS is a full featured server for World of Warcraft, supporting
 * the following clients: 1.12.x, 2.4.3, 3.3.5a, 4.3.4a and 5.4.8
 *
 * Copyright (C) 2005-2026 MaNGOS <https://www.getmangos.eu>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 * World of Warcraft, and all World of Warcraft or Warcraft art, images,
 * and lore are copyrighted by Blizzard Entertainment, Inc.
 */

#include "Util.h"
#include "Timer.h"

#include "utf8.h"
#include "RNGen.h"
#include "Log/Log.h"

#include <iomanip>
#include <cctype>
#include <cstdarg>
#include <cstring>

#ifndef _WIN32
#  include <arpa/inet.h>                                    ///< inet_addr
#endif

//static ACE_Time_Value g_SystemTickTime = ACE_OS::gettimeofday();

//uint32 WorldTimer::m_iTime = 0;
//uint32 WorldTimer::m_iPrevTime = 0;
//
//uint32 WorldTimer::tickTime() { return m_iTime; }
//uint32 WorldTimer::tickPrevTime() { return m_iPrevTime; }
//
//uint32 WorldTimer::tick()
//{
//    // save previous world tick time
//    m_iPrevTime = m_iTime;
//
//    // get the new one and don't forget to persist current system time in m_SystemTickTime
//    m_iTime = WorldTimer::getMSTime_internal();
//
//    // return tick diff
//    return getMSTimeDiff(m_iPrevTime, m_iTime);
//}
//
//uint32 WorldTimer::getMSTime()
//{
//    return getMSTime_internal();
//}
//
//uint32 WorldTimer::getMSTime_internal()
//{
//    // get current time
//    const ACE_Time_Value currTime = ACE_OS::gettimeofday();
//    // calculate time diff between two world ticks
//    // special case: curr_time < old_time - we suppose that our time has not ticked at all
//    // this should be constant value otherwise it is possible that our time can start ticking backwards until next world tick!!!
//    uint64 diff = 0;
//    (currTime - g_SystemTickTime).msec(diff);
//
//    // lets calculate current world time
//    uint32 iRes = uint32(diff % UI64LIT(0x00000000FFFFFFFF));
//    return iRes;
//}

//////////////////////////////////////////////////////////////////////////
int32 irand(int32 min, int32 max)
{
    return RNG::instance()->rand_i(min,max);
}

uint32 urand(uint32 min, uint32 max)
{
    return RNG::instance()->rand_u(min,max);
}

float frand(float min, float max)
{
    return RNG::instance()->rand_f(min, max);
}

int32 rand32()
{
    return RNG::instance()->rand();
}

double rand_norm(void)
{
    return RNG::instance()->rand_d(0.0, 1.0);
}

float rand_norm_f(void)
{
    return RNG::instance()->rand_f(0.0, 1.0);
}

double rand_chance(void)
{
    return RNG::instance()->rand_d(0.0, 100.0);
}

float rand_chance_f(void)
{
    return RNG::instance()->rand_f(0.0, 100.0);
}

Tokens StrSplit(const std::string& src, const std::string& sep)
{
    Tokens r;
    std::string s;
    for (std::string::const_iterator i = src.begin(); i != src.end(); ++i)
    {
        if (sep.find(*i) != std::string::npos)
        {
            if (s.length())
            {
                r.push_back(s);
            }
            s = "";
        }
        else
        {
            s += *i;
        }
    }
    if (s.length())
    {
        r.push_back(s);
    }
    return r;
}

uint32 GetUInt32ValueFromArray(Tokens const& data, uint16 index)
{
    if (index >= data.size())
    {
        return 0;
    }

    return (uint32)atoi(data[index].c_str());
}

float GetFloatValueFromArray(Tokens const& data, uint16 index)
{
    float result;
    uint32 temp = GetUInt32ValueFromArray(data, index);
    memcpy(&result, &temp, sizeof(result));

    return result;
}

// modulos a radian orientation to the range of 0..2PI
float NormalizeOrientation(float o)
{
    // fmod only supports positive numbers. Thus we have
    // to emulate negative numbers
    if (o < 0)
    {
        float mod = o * -1;
        mod = fmod(mod, 2.0f * M_PI_F);
        mod = -mod + 2.0f * M_PI_F;
        return mod;
    }
    return fmod(o, 2.0f * M_PI_F);
}

void StripRealmSuffix(std::string& name, std::string const& realmName)
{
    if (realmName.empty())
    {
        return;
    }

    std::string const suffix = "-" + realmName;
    if (name.size() <= suffix.size())                       // "-Realm" alone is not a name
    {
        return;
    }

    size_t const at = name.size() - suffix.size();
    for (size_t i = 0; i < suffix.size(); ++i)
    {
        if (std::tolower(static_cast<unsigned char>(name[at + i])) !=
            std::tolower(static_cast<unsigned char>(suffix[i])))
        {
            return;
        }
    }
    name.resize(at);
}

void stripLineInvisibleChars(std::string& str)
{
    static std::string invChars = " \t\7\n";

    size_t wpos = 0;

    bool space = false;
    for (size_t pos = 0; pos < str.size(); ++pos)
    {
        if (invChars.find(str[pos]) != std::string::npos)
        {
            if (!space)
            {
                str[wpos++] = ' ';
                space = true;
            }
        }
        else
        {
            if (wpos != pos)
            {
                str[wpos++] = str[pos];
            }
            else
            {
                ++wpos;
            }
            space = false;
        }
    }

    if (wpos < str.size())
    {
        str.erase(wpos, str.size());
    }
}

/**
 * It's a wrapper for the localtime_r function that works on Windows
 *
 * @param time The time to convert.
 * @param result A pointer to a tm structure to receive the broken-down time.
 *
 * @return A pointer to the result.
 */
#if (defined(WIN32) || defined(_WIN32) || defined(__WIN32__))
struct tm* localtime_r(time_t const* time, struct tm *result)
{
    localtime_s(result, time);
    return result;
}
#endif

/**
 * It takes a time_t value and returns a tm structure with the same time, but in local time
 *
 * @param time The time to break down.
 *
 * @return A struct tm
 */
tm TimeBreakdown(time_t time)
{
    tm timeLocal;
    localtime_r(&time, &timeLocal);
    return timeLocal;
}

/**
 * Convert local time to UTC time.
 *
 * @param time The time to convert.
 *
 * @return The time in UTC.
 */
time_t LocalTimeToUTCTime(time_t time)
{
    #if (defined(WIN32) || defined(_WIN32) || defined(__WIN32__))
        return time + _timezone;
    #else
        return time + timezone;
    #endif
}

/**
 * "Get the timestamp of the next time the given hour occurs in the local timezone."
 *
 * The function takes a timestamp, an hour, and a boolean. The timestamp is the time you want to find
 * the next occurrence of the given hour. The hour is the hour you want to find the next occurrence of.
 * The boolean is whether or not you want to find the next occurrence of the hour after the given
 * timestamp
 *
 * @param time The time you want to get the hour timestamp for.
 * @param hour The hour of the day you want to get the timestamp for.
 * @param onlyAfterTime If true, the function will return the next hour after the current time. If
 * false, it will return the current hour.
 *
 * @return A timestamp for the given hour of the day.
 */
time_t GetLocalHourTimestamp(time_t time, uint8 hour, bool onlyAfterTime)
{
    tm timeLocal = TimeBreakdown(time);
    timeLocal.tm_hour = 0;
    timeLocal.tm_min  = 0;
    timeLocal.tm_sec  = 0;

    time_t midnightLocal = mktime(&timeLocal);
    time_t hourLocal = midnightLocal + hour * HOUR;

    if (onlyAfterTime && hourLocal < time)
    {
        hourLocal += DAY;
    }

    return hourLocal;
}

std::string secsToTimeString(time_t timeInSecs, TimeFormat timeFormat, bool hoursOnly)
{
    time_t secs    = timeInSecs % MINUTE;
    time_t minutes = timeInSecs % HOUR / MINUTE;
    time_t hours   = timeInSecs % DAY  / HOUR;
    time_t days    = timeInSecs / DAY;

    std::ostringstream ss;
    if (days)
    {
        ss << days;
        if (timeFormat == TimeFormat::Numeric)
        {
            ss << ":";
        }
        else if (timeFormat == TimeFormat::ShortText)
        {
            ss << "d";
        }
        else // if (timeFormat == TimeFormat::FullText)
        {
            if (days == 1)
            {
                ss << " Day ";
            }
            else
            {
                ss << " Days ";
            }
        }
    }

    if (hours || hoursOnly)
    {
        ss << hours;
        if (timeFormat == TimeFormat::Numeric)
        {
            ss << ":";
        }
        else if (timeFormat == TimeFormat::ShortText)
        {
            ss << "h";
        }
        else // if (timeFormat == TimeFormat::FullText)
        {
            if (hours <= 1)
            {
                ss << " Hour ";
            }
            else
            {
                ss << " Hours ";
            }
        }
    }

    if (!hoursOnly)
    {
        ss << minutes;
        if (timeFormat == TimeFormat::Numeric)
        {
            ss << ":";
        }
        else if (timeFormat == TimeFormat::ShortText)
        {
            ss << "m";
        }
        else // if (timeFormat == TimeFormat::FullText)
        {
            if (minutes == 1)
            {
                ss << " Minute ";
            }
            else
            {
                ss << " Minutes ";
            }
        }
    }
    else
    {
        if (timeFormat == TimeFormat::Numeric)
        {
            ss << "0:";
        }
    }

    if (secs || (!days && !hours && !minutes))
    {
        ss << std::setw(2) << std::setfill('0') << secs;
        if (timeFormat == TimeFormat::ShortText)
        {
            ss << "s";
        }
        else if (timeFormat == TimeFormat::FullText)
        {
            if (secs <= 1)
            {
                ss << " Second.";
            }
            else
            {
                ss << " Seconds.";
            }
        }
    }
    else
    {
        if (timeFormat == TimeFormat::Numeric)
        {
            ss << "00";
        }
    }

    return ss.str();
}

uint32 TimeStringToSecs(const std::string& timestring)
{
    uint32 secs       = 0;
    uint32 buffer     = 0;
    uint32 multiplier = 0;

    for (std::string::const_iterator itr = timestring.begin(); itr != timestring.end(); ++itr)
    {
        if (isdigit(*itr))
        {
            buffer *= 10;
            buffer += (*itr) - '0';
        }
        else
        {
            switch (*itr)
            {
                case 'd': multiplier = DAY;     break;
                case 'h': multiplier = HOUR;    break;
                case 'm': multiplier = MINUTE;  break;
                case 's': multiplier = 1;       break;
                default : return 0;                         // bad format
            }
            buffer *= multiplier;
            secs += buffer;
            buffer = 0;
        }
    }

    return secs;
}

std::string TimeToTimestampStr(time_t t)
{
    tm aTm;
    localtime_r(&t, &aTm);
    //       YYYY   year
    //       MM     month (2 digits 01-12)
    //       DD     day (2 digits 01-31)
    //       HH     hour (2 digits 00-23)
    //       MM     minutes (2 digits 00-59)
    //       SS     seconds (2 digits 00-59)
    char buf[20];
    snprintf(buf, 20, "%04d-%02d-%02d_%02d-%02d-%02d", aTm.tm_year + 1900, aTm.tm_mon + 1, aTm.tm_mday, aTm.tm_hour, aTm.tm_min, aTm.tm_sec);
    return std::string(buf);
}

time_t timeBitFieldsToSecs(uint32 packedDate)
{
    tm lt;
    memset(&lt, 0, sizeof(lt));

    lt.tm_min = packedDate & 0x3F;
    lt.tm_hour = (packedDate >> 6) & 0x1F;
    lt.tm_wday = (packedDate >> 11) & 7;
    lt.tm_mday = ((packedDate >> 14) & 0x3F) + 1;
    lt.tm_mon = (packedDate >> 20) & 0xF;
    lt.tm_year = ((packedDate >> 24) & 0x1F) + 100;
    // -1 lets mktime resolve DST for this date. The memset above had forced 0,
    // i.e. standard time always -- while the encode side, secsToTimeBitFields,
    // uses localtime() and does honour DST. That asymmetry meant decode was not
    // the inverse of encode: a summer event entered at 00:00 came back as 01:00
    // on a DST-observing server. Same fix as mangostwo/server#210.
    lt.tm_isdst = -1;

    return time_t(mktime(&lt));
}

std::string MoneyToString(uint64 money)
{
    uint32 gold = money / 10000;
    uint32 silv = (money % 10000) / 100;
    uint32 copp = (money % 10000) % 100;
    std::stringstream ss;
    if (gold)
    {
        ss << gold << "g";
    }
    if (silv || gold)
    {
        ss << silv << "s";
    }
    ss << copp << "c";

    return ss.str();
}

/// Check if the string is a valid ip address representation
bool IsIPAddress(char const* ipaddress)
{
    if (!ipaddress)
    {
        return false;
    }

    // Let the big boys do it.
    // Drawback: all valid ip address formats are recognized e.g.: 12.23,121234,0xABCD)
    return inet_addr(ipaddress) != INADDR_NONE;
}

std::string GetAddressString(uint32 ip, uint16 port)
{
    char buf[32];
    snprintf(buf, sizeof(buf), "%u.%u.%u.%u:%u",
             (ip >> 24) & 0xFF, (ip >> 16) & 0xFF, (ip >> 8) & 0xFF, ip & 0xFF,
             unsigned(port));
    return buf;
}

bool IsIPAddrInNetwork(uint32 net, uint32 addr, uint32 subnetMask)
{
    return (net & subnetMask) == (addr & subnetMask);
}

/// create PID file
uint32 CreatePIDFile(const std::string& filename)
{
    FILE* pid_file = fopen(filename.c_str(), "w");
    if (pid_file == NULL)
    {
        return 0;
    }

#ifdef WIN32
    DWORD pid = GetCurrentProcessId();
#else
    pid_t pid = getpid();
#endif

    fprintf(pid_file, "%d", pid);
    fclose(pid_file);

    return (uint32)pid;
}

size_t utf8length(std::string& utf8str)
{
    try
    {
        return utf8::distance(utf8str.c_str(), utf8str.c_str() + utf8str.size());
    }
    catch (std::exception)
    {
        utf8str = "";
        return 0;
    }
}

void utf8truncate(std::string& utf8str, size_t len)
{
    try
    {
        size_t wlen = utf8::distance(utf8str.c_str(), utf8str.c_str() + utf8str.size());
        if (wlen <= len)
        {
            return;
        }

        std::wstring wstr;
        wstr.resize(wlen);
        utf8::utf8to16(utf8str.c_str(), utf8str.c_str() + utf8str.size(), &wstr[0]);
        wstr.resize(len);
        char* oend = utf8::utf16to8(wstr.c_str(), wstr.c_str() + wstr.size(), &utf8str[0]);
        utf8str.resize(oend - (&utf8str[0]));               // remove unused tail
    }
    catch (std::exception)
    {
        utf8str = "";
    }
}

bool Utf8ToUpperOnlyLatin(std::string& utf8String)
{
    std::wstring wstr;
    if (!Utf8toWStr(utf8String, wstr))
    {
        return false;
    }

    std::transform(wstr.begin(), wstr.end(), wstr.begin(), wcharToUpperOnlyLatin);

    return WStrToUtf8(wstr, utf8String);
}

bool Utf8toWStr(char const* utf8str, size_t csize, wchar_t* wstr, size_t& wsize)
{
    try
    {
        size_t len = utf8::distance(utf8str, utf8str + csize);
        if (len > wsize)
        {
            if (wsize > 0)
            {
                wstr[0] = L'\0';
            }
            wsize = 0;
            return false;
        }

        wsize = len;
        utf8::utf8to16(utf8str, utf8str + csize, wstr);
        wstr[len] = L'\0';
    }
    catch (std::exception)
    {
        if (wsize > 0)
        {
            wstr[0] = L'\0';
        }
        wsize = 0;
        return false;
    }

    return true;
}

bool Utf8toWStr(const std::string& utf8str, std::wstring& wstr)
{
    try
    {
        size_t len = utf8::distance(utf8str.c_str(), utf8str.c_str() + utf8str.size());
        wstr.resize(len);

        if (len)
        {
            utf8::utf8to16(utf8str.c_str(), utf8str.c_str() + utf8str.size(), &wstr[0]);
        }
    }
    catch (std::exception)
    {
        wstr = L"";
        return false;
    }

    return true;
}

bool WStrToUtf8(wchar_t* wstr, size_t size, std::string& utf8str)
{
    try
    {
        std::string utf8str2;
        utf8str2.resize(size * 4);                          // allocate for most long case

        char* oend = utf8::utf16to8(wstr, wstr + size, &utf8str2[0]);
        utf8str2.resize(oend - (&utf8str2[0]));             // remove unused tail
        utf8str = utf8str2;
    }
    catch (std::exception)
    {
        utf8str = "";
        return false;
    }

    return true;
}

bool WStrToUtf8(std::wstring wstr, std::string& utf8str)
{
    try
    {
        std::string utf8str2;
        utf8str2.resize(wstr.size() * 4);                   // allocate for most long case

        char* oend = utf8::utf16to8(wstr.c_str(), wstr.c_str() + wstr.size(), &utf8str2[0]);
        utf8str2.resize(oend - (&utf8str2[0]));             // remove unused tail
        utf8str = utf8str2;
    }
    catch (std::exception)
    {
        utf8str = "";
        return false;
    }

    return true;
}

typedef wchar_t const* const* wstrlist;

std::wstring GetMainPartOfName(std::wstring wname, uint32 declension)
{
    // supported only Cyrillic cases
    if (wname.size() < 1 || !isCyrillicCharacter(wname[0]) || declension > 5)
    {
        return wname;
    }

    // Important: end length must be <= MAX_INTERNAL_PLAYER_NAME-MAX_PLAYER_NAME (3 currently)

    static wchar_t const a_End[]    = { wchar_t(1), wchar_t(0x0430), wchar_t(0x0000)};
    static wchar_t const o_End[]    = { wchar_t(1), wchar_t(0x043E), wchar_t(0x0000)};
    static wchar_t const ya_End[]   = { wchar_t(1), wchar_t(0x044F), wchar_t(0x0000)};
    static wchar_t const ie_End[]   = { wchar_t(1), wchar_t(0x0435), wchar_t(0x0000)};
    static wchar_t const i_End[]    = { wchar_t(1), wchar_t(0x0438), wchar_t(0x0000)};
    static wchar_t const yeru_End[] = { wchar_t(1), wchar_t(0x044B), wchar_t(0x0000)};
    static wchar_t const u_End[]    = { wchar_t(1), wchar_t(0x0443), wchar_t(0x0000)};
    static wchar_t const yu_End[]   = { wchar_t(1), wchar_t(0x044E), wchar_t(0x0000)};
    static wchar_t const oj_End[]   = { wchar_t(2), wchar_t(0x043E), wchar_t(0x0439), wchar_t(0x0000)};
    static wchar_t const ie_j_End[] = { wchar_t(2), wchar_t(0x0435), wchar_t(0x0439), wchar_t(0x0000)};
    static wchar_t const io_j_End[] = { wchar_t(2), wchar_t(0x0451), wchar_t(0x0439), wchar_t(0x0000)};
    static wchar_t const o_m_End[]  = { wchar_t(2), wchar_t(0x043E), wchar_t(0x043C), wchar_t(0x0000)};
    static wchar_t const io_m_End[] = { wchar_t(2), wchar_t(0x0451), wchar_t(0x043C), wchar_t(0x0000)};
    static wchar_t const ie_m_End[] = { wchar_t(2), wchar_t(0x0435), wchar_t(0x043C), wchar_t(0x0000)};
    static wchar_t const soft_End[] = { wchar_t(1), wchar_t(0x044C), wchar_t(0x0000)};
    static wchar_t const j_End[]    = { wchar_t(1), wchar_t(0x0439), wchar_t(0x0000)};

    static wchar_t const* const dropEnds[6][8] =
    {
        { &a_End[1],  &o_End[1],    &ya_End[1],   &ie_End[1],  &soft_End[1], &j_End[1],    NULL,       NULL },
        { &a_End[1],  &ya_End[1],   &yeru_End[1], &i_End[1],   NULL,         NULL,         NULL,       NULL },
        { &ie_End[1], &u_End[1],    &yu_End[1],   &i_End[1],   NULL,         NULL,         NULL,       NULL },
        { &u_End[1],  &yu_End[1],   &o_End[1],    &ie_End[1],  &soft_End[1], &ya_End[1],   &a_End[1],  NULL },
        { &oj_End[1], &io_j_End[1], &ie_j_End[1], &o_m_End[1], &io_m_End[1], &ie_m_End[1], &yu_End[1], NULL },
        { &ie_End[1], &i_End[1],    NULL,         NULL,        NULL,         NULL,         NULL,       NULL }
    };

    for (wchar_t const * const* itr = &dropEnds[declension][0]; *itr; ++itr)
    {
        size_t len = size_t((*itr)[-1]);                    // get length from string size field

        if (wname.substr(wname.size() - len, len) == *itr)
        {
            return wname.substr(0, wname.size() - len);
        }
    }

    return wname;
}


bool utf8ToConsole(const std::string& utf8str, std::string& conStr)
{
#if PLATFORM == PLATFORM_WINDOWS
    std::wstring wstr;
    if (!Utf8toWStr(utf8str, wstr))
    {
        return false;
    }

    conStr.resize(wstr.size());
    CharToOemBuffW(&wstr[0], &conStr[0], wstr.size());
#else
    // not implemented yet
    conStr = utf8str;
#endif

    return true;
}

bool consoleToUtf8(const std::string& conStr, std::string& utf8str)
{
#if PLATFORM == PLATFORM_WINDOWS
    std::wstring wstr;
    wstr.resize(conStr.size());
    OemToCharBuffW(&conStr[0], &wstr[0], conStr.size());

    return WStrToUtf8(wstr, utf8str);
#else
    // not implemented yet
    utf8str = conStr;
    return true;
#endif
}

bool Utf8FitTo(const std::string& str, std::wstring search)
{
    std::wstring temp;

    if (!Utf8toWStr(str, temp))
    {
        return false;
    }

    // converting to lower case
    wstrToLower(temp);

    if (temp.find(search) == std::wstring::npos)
    {
        return false;
    }

    return true;
}

void vutf8printf(FILE* out, const char* str, va_list* ap)
{
#if PLATFORM == PLATFORM_WINDOWS
    char temp_buf[32 * 1024];
    wchar_t wtemp_buf[32 * 1024];

    size_t temp_len = vsnprintf(temp_buf, 32 * 1024, str, *ap);

    size_t wtemp_len = 32 * 1024 - 1;
    Utf8toWStr(temp_buf, temp_len, wtemp_buf, wtemp_len);

    CharToOemBuffW(&wtemp_buf[0], &temp_buf[0], wtemp_len + 1);
    fprintf(out, "%s", temp_buf);
#else
    vfprintf(out, str, *ap);
#endif
}

std::string vutf8format(const char* str, va_list* ap)
{
#if PLATFORM == PLATFORM_WINDOWS
    char temp_buf[32 * 1024];
    wchar_t wtemp_buf[32 * 1024];

    size_t temp_len = vsnprintf(temp_buf, 32 * 1024, str, *ap);
    if (temp_len >= 32 * 1024)
    {
        temp_len = 32 * 1024 - 1;
    }

    size_t wtemp_len = 32 * 1024 - 1;
    Utf8toWStr(temp_buf, temp_len, wtemp_buf, wtemp_len);

    CharToOemBuffW(&wtemp_buf[0], &temp_buf[0], wtemp_len + 1);
    return std::string(temp_buf);
#else
    char temp_buf[32 * 1024];
    va_list ap_copy;
    va_copy(ap_copy, *ap);
    int n = vsnprintf(temp_buf, sizeof(temp_buf), str, ap_copy);
    va_end(ap_copy);
    if (n < 0)
    {
        return std::string();
    }
    if (size_t(n) < sizeof(temp_buf))
    {
        return std::string(temp_buf, size_t(n));
    }
    // Message is longer than the stack buffer: render it in full into an
    // exactly-sized string rather than truncating, matching the legacy
    // unbounded vfprintf path. vsnprintf returned the length it WOULD have
    // written; allocate that and reformat from a fresh va_list copy.
    std::string big(size_t(n), '\0');
    va_copy(ap_copy, *ap);
    vsnprintf(&big[0], big.size() + 1, str, ap_copy);
    va_end(ap_copy);
    return big;
#endif

}

// Kept free of C++ objects on purpose: MSVC rejects __try in a function that
// requires object unwinding (C2712), so the guarded call gets its own frame.
static bool _GuardedVsnprintf(char* buffer, size_t size, char const* format, va_list ap)
{
    // Gated on the compiler, not the OS: __try/__except is MSVC syntax, and a
    // MinGW build targets Windows without supporting it.
    // The return value is deliberately not treated as a success flag: _vsnprintf
    // returns -1 for ordinary truncation as well as for a rejected format, so
    // testing it would drop every message merely too long for its buffer. Formats
    // the CRT would reject are refused by the grammar check in the caller instead,
    // which is why that check only accepts conversions the local CRT implements.
#ifdef _MSC_VER
    __try
    {
        vsnprintf(buffer, size, format, ap);
        return true;
    }
    // Only an access violation is expected here (a conversion dereferencing an
    // argument that was never passed). Anything else keeps unwinding rather than
    // being silently swallowed.
    __except (GetExceptionCode() == EXCEPTION_ACCESS_VIOLATION ? EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH)
    {
        return false;
    }
#else
    // No portable equivalent: a malformed format is still fatal here, but the
    // callers' reporting and the load-time checks in ObjectMgr apply on every
    // platform.
    return vsnprintf(buffer, size, format, ap) >= 0;
#endif
}

bool SafeFormatDbString(char* buffer, size_t size, char const* format, va_list ap)
{
    if (!buffer || !size)
    {
        return false;
    }

    buffer[0] = '\0';

    if (!format)
    {
        return false;
    }

    // Two conversions must never reach vsnprintf, and neither can be contained
    // afterwards:
    //   %n writes a count through an argument pointer, and UCRT answers it by
    //   invoking the invalid-parameter handler, which terminates rather than raising
    //   anything an exception filter could catch;
    //   a '%' followed by something that is not a valid conversion - including a
    //   trailing '%' - reaches that same handler.
    // Refusing them here is also why the load-time check never has to rewrite a row:
    // `script_texts` carries a %n only as a typo for the $n name token, and that text
    // is spoken verbatim elsewhere where it reads perfectly well.
    for (char const* p = format; *p; ++p)
    {
        if (*p != '%')
        {
            continue;
        }

        ++p;

        if (*p == '%')
        {
            continue;                                       // "%%" is a literal percent
        }

        // The field must match %[flags][width][.precision][size]type exactly. Anything
        // else - a stray '.', a positional "%1$s" (which belongs to the _sprintf_p
        // family, not the _vsnprintf this build maps to), or a truncated field -
        // reaches the invalid-parameter handler too.
        while (*p == '-' || *p == '+' || *p == ' ' || *p == '#' || *p == '0')
        {
            ++p;
        }

        if (*p == '*')
        {
            ++p;
        }
        else
        {
            while (*p >= '0' && *p <= '9')
            {
                ++p;
            }
        }

        if (*p == '.')
        {
            ++p;

            if (*p == '*')
            {
                ++p;
            }
            else
            {
                while (*p >= '0' && *p <= '9')
                {
                    ++p;
                }
            }
        }

        // Size prefixes, longest match first.
        //
        // "I", "I32", "I64" and "w" are MSVC spellings that glibc does not implement:
        // there the I is read as a flag and the 32/64 as a field width, so %I64u
        // consumes an unsigned int and silently truncates anything above 32 bits.
        // Accepting them only where the CRT understands them stops a row that renders
        // correctly on Windows from rendering wrong numbers on Linux.
#ifdef _MSC_VER
        bool const msvcSizePrefixes = true;
#else
        bool const msvcSizePrefixes = false;
#endif

        char sizePrefix = '\0';                             // '\0' = none

        if (msvcSizePrefixes && p[0] == 'I' && ((p[1] == '3' && p[2] == '2') || (p[1] == '6' && p[2] == '4')))
        {
            sizePrefix = p[1];                              // '3' for I32, '6' for I64
            p += 3;
        }
        else if ((p[0] == 'h' && p[1] == 'h') || (p[0] == 'l' && p[1] == 'l'))
        {
            sizePrefix = (p[0] == 'h') ? 'H' : 'Q';         // 'H' = hh, 'Q' = ll (not 'L',
                                                            // which is the distinct long-double modifier)
            p += 2;
        }
        else if (*p == 'h' || *p == 'l' || *p == 'L' || *p == 'j' || *p == 'z' || *p == 't')
        {
            sizePrefix = *p;
            ++p;
        }
        else if (msvcSizePrefixes && (*p == 'I' || *p == 'w'))
        {
            sizePrefix = *p;
            ++p;
        }

        // '\0' is tested first: strchr would otherwise match the set's own terminator
        // The accepted set is what the LOCAL CRT implements, not the union of every
        // CRT. C, S and Z are MSVC spellings: glibc rejects them, and a rejected
        // conversion makes vsnprintf return -1 having written nothing useful, which
        // is indistinguishable here from ordinary truncation. Refusing them up front
        // is what lets the call below ignore its return value safely.
        // C and S are available on both: MSVC spells the wide forms that way, and
        // glibc keeps them as XSI aliases for %lc and %ls. Z is the one that is
        // genuinely MSVC-only, and glibc rejects it.
#ifdef _MSC_VER
        char const* const acceptedConversions = "diouxXfFeEgGaAcCsSpZ";
#else
        char const* const acceptedConversions = "diouxXfFeEgGaAcCsSp";
#endif

        // A size prefix is only meaningful for some conversions. "%I64s", "%zs" and
        // "%Ld" satisfy the grammar piecewise but are not valid pairings: MSVC answers
        // them with the invalid-parameter handler, which terminates and is precisely
        // what the SEH block cannot contain, and a CRT that tolerates one instead reads
        // the argument as the wrong type and returns success with garbage.
        if (sizePrefix != '\0' && *p != '\0')
        {
            bool const integerConversion = strchr("diouxX", *p) != nullptr;
            bool const floatConversion   = strchr("fFeEgGaA", *p) != nullptr;
            bool const stringConversion  = (*p == 's' || *p == 'c');
            bool valid = false;

            switch (sizePrefix)
            {
                case 'H':                                   // hh
                case 'Q':                                   // ll
                case 'j':                                   // intmax_t
                case 'z':                                   // size_t
                case 't':                                   // ptrdiff_t
                case '3':                                   // I32
                case '6':                                   // I64
                case 'I':                                   // pointer-width
                    valid = integerConversion;
                    break;
                case 'h':                                   // short, or single-byte s/c on MSVC
                    valid = integerConversion || (msvcSizePrefixes && stringConversion);
                    break;
                case 'l':                                   // long, wide s/c, or a no-op
                    // before a float: %lf, %le and %lg are valid and consume the same
                    // promoted double as the unmodified conversion.
                    valid = integerConversion || stringConversion || floatConversion;
                    break;
                case 'L':                                   // long double ONLY.
                    // Not integers: UCRT answers %Ld with the invalid-parameter
                    // handler, which raises STATUS_STACK_BUFFER_OVERRUN through
                    // __fastfail and cannot be caught by the SEH block below.
                    // Measured, not assumed - it terminated the test harness.
                    valid = floatConversion;
                    break;
                case 'w':                                   // MSVC wide s/c
                    valid = stringConversion;
                    break;
                default:
                    valid = false;
                    break;
            }

            if (!valid)
            {
                return false;
            }
        }

        if (*p == '\0' || !strchr(acceptedConversions, *p))
        {
            return false;                                   // %n, trailing '%', or malformed
        }
    }

    bool const formatted = _GuardedVsnprintf(buffer, size, format, ap);

    // _vsnprintf does not terminate when the output is truncated, and a faulted
    // call may have written a partial result.
    buffer[size - 1] = '\0';

    if (!formatted)
    {
        buffer[0] = '\0';
    }

    return formatted;
}

bool SafeFormatDbStringF(char* buffer, size_t size, char const* format, ...)
{
    va_list ap;
    va_start(ap, format);
    bool const formatted = SafeFormatDbString(buffer, size, format, ap);
    va_end(ap);

    return formatted;
}

void CopyDbStringBounded(char* buffer, size_t size, char const* text)
{
    if (!buffer || !size)
    {
        return;
    }

    if (!text)
    {
        buffer[0] = '\0';
        return;
    }

    strncpy(buffer, text, size - 1);
    buffer[size - 1] = '\0';
}

void hexEncodeByteArray(uint8* bytes, uint32 arrayLen, std::string& result)
{
    std::ostringstream ss;
    for (uint32 i = 0; i < arrayLen; ++i)
    {
        for (uint8 j = 0; j < 2; ++j)
        {
            unsigned char nibble = 0x0F & (bytes[i] >> ((1 - j) * 4));
            char encodedNibble;
            if (nibble < 0x0A)
            {
                encodedNibble = '0' + nibble;
            }
            else
            {
                encodedNibble = 'A' + nibble - 0x0A;
            }
            ss << encodedNibble;
        }
    }
    result = ss.str();
}

std::string ByteArrayToHexStr(uint8 const* bytes, uint32 arrayLen, bool reverse /* = false */)
{
    int32 init = 0;
    int32 end = arrayLen;
    int8 op = 1;

    if (reverse)
    {
        init = arrayLen - 1;
        end = -1;
        op = -1;
    }

    std::ostringstream ss;
    for (int32 i = init; i != end; i += op)
    {
        char buffer[4];
        sprintf(buffer, "%02X", bytes[i]);
        ss << buffer;
    }

    return ss.str();
}

void HexStrToByteArray(std::string const& str, uint8* out, bool reverse /*= false*/)
{
    // string must have even number of characters
    if (str.length() & 1)
    {
        return;
    }

    int32 init = 0;
    int32 end = str.length();
    int8 op = 1;

    if (reverse)
    {
        init = str.length() - 2;
        end = -2;
        op = -1;
    }

    uint32 j = 0;
    for (int32 i = init; i != end; i += 2 * op)
    {
        char buffer[3] = { str[i], str[i + 1], '\0' };
        out[j++] = strtoul(buffer, NULL, 16);
    }
}

void utf8print(void* /*arg*/, const char* str)
{
#if PLATFORM == PLATFORM_WINDOWS
    wchar_t wtemp_buf[6000];
    size_t wtemp_len = 6000 - 1;
    if (!Utf8toWStr(str, strlen(str), wtemp_buf, wtemp_len))
    {
        return;
    }

    char temp_buf[6000];
    CharToOemBuffW(&wtemp_buf[0], &temp_buf[0], wtemp_len + 1);
    // Route CLI command output through the console writer (verbatim) so it
    // shares the single serialized stdout with bar redraws / log lines and
    // cannot tear against or overtake them.
    sLog.ConsoleEmitRaw(temp_buf);
#else
    sLog.ConsoleEmitRaw(str);
#endif
}

void utf8printf(FILE* out, const char* str, ...)
{
    va_list ap;
    va_start(ap, str);
    vutf8printf(out, str, &ap);
    va_end(ap);
}

int return_iCoreNumber()
{
#if defined(CLASSIC)
    return 0;
#elif defined(TBC)
    return 1;
#elif defined(WOTLK)
    return 2;
#elif defined(CATA)
    return 3;
#elif defined(MOP) || defined(MISTS)
    // NewMangosFour's canonical CMake expansion name is MISTS.
    return 4;
#elif defined(WOD)
    return 5;
#elif defined(LEGION)
    return 6;
#else
    return -1;
#endif
}

/// Print out the core banner
void print_banner()
{
    int iCoreNumber = return_iCoreNumber();
    switch (iCoreNumber)
    {
    case 0: // CLASSIC
        sLog.outString("<Ctrl-C> to stop.\n"
            "  __  __      _  _  ___  ___  ___        ____              \n"
            " |  \\/  |__ _| \\| |/ __|/ _ \\/ __|      /_  /___ _ _ ___   \n"
            " | |\\/| / _` | .` | (_ | (_) \\__ \\       / // -_) '_/ _ \\ \n"
            " |_|  |_\\__,_|_|\\_|\\___|\\___/|___/      /___\\___|_| \\___/\n"
            " Powered By MaNGOS Core\n"
            "__________________________________________________________\n"
            "\n"
            "Website/Forum/Wiki/Issue Tracker: https://www.getmangos.eu\n"
            "__________________________________________________________\n"
            "\n");
        break;
    case 1: // TBC
        sLog.outString("<Ctrl-C> to stop.\n"
            "  __  __      _  _  ___  ___  ___         ___             \n"
            " |  \\/  |__ _| \\| |/ __|/ _ \\/ __|       / _ \\ ___  ___  \n"
            " | |\\/| / _` | .` | (_ | (_) \\__ \\      | (_) |   \\/ -_) \n"
            " |_|  |_\\__,_|_|\\_|\\___|\\___/|___/       \\___/|_||_\\___|\n"
            " Powered By MaNGOS Core\n"
            " __________________________________________________________\n"
            "\n"
            " Website/Forum/Wiki/Issue Tracker: https://www.getmangos.eu\n"
            " __________________________________________________________\n"
            "\n");
        break;
    case 2: // WOTLK
        sLog.outString("<Ctrl-C> to stop.\n"
            "  __  __      _  _  ___  ___  ___       _____          \n"
            " |  \\/  |__ _| \\| |/ __|/ _ \\/ __|     |_   _|_ __ _____\n"
            " | |\\/| / _` | .` | (_ | (_) \\__ \\       | | \\ V  V / _ \\\n"
            " |_|  |_\\__,_|_|\\_|\\___|\\___/|___/       |_|  \\_/\\_/\\___/ \n"
            " Powered By MaNGOS Core\n"
            " __________________________________________________________\n"
            "\n"
            " Website/Forum/Wiki/Issue Tracker: https://www.getmangos.eu\n"
            " __________________________________________________________\n"
            "\n");
        break;
    case 3: // CATA
        sLog.outString("<Ctrl-C> to stop.\n"
            "  __  __      _  _  ___  ___  ___   _____ _         \n"
            " |  \\/  |__ _| \\| |/ __|/ _ \\/ __| |_   _| |_  _ _ ___ ___    \n"
            " | |\\/| / _` | .` | (_ | (_) \\__ \\   | | | ' \\| '_/ -_) -_)  \n"
            " |_|  |_\\__,_|_|\\_|\\___|\\___/|___/   |_| |_||_|_| \\___\\___| \n"
            " Powered By MaNGOS Core\n"
            " __________________________________________________________\n"
            "\n"
            " Website/Forum/Wiki/Issue Tracker: https://www.getmangos.eu\n"
            " __________________________________________________________\n"
            "\n");
        break;
    case 4: // MOP
        sLog.outString("<Ctrl-C> to stop.\n"
            "  __  __      _  _  ___  ___  ___     _____             \n"
            " |  \\/  |__ _| \\| |/ __|/ _ \\/ __|    | __|__ _  _ _ _  \n"
            " | |\\/| / _` | .` | (_ | (_) \\__ \\    | _/ _ \\ || | '_|\n"
            " |_|  |_\\__,_|_|\\_|\\___|\\___/|___/    |_|\\___/\\_,_|_| \n"
            " Powered By MaNGOS Core\n"
            " __________________________________________________________\n"
            "\n"
            " Website/Forum/Wiki/Issue Tracker: https://www.getmangos.eu\n"
            " __________________________________________________________\n"
            "\n");
        break;
    default:
        sLog.outString("<Ctrl-C> to stop.\n"
            "  __  __      _  _  ___  ___  ___                                \n"
            " |  \\/  |__ _| \\| |/ __|/ _ \\/ __|     We have a problem !   \n"
            " | |\\/| / _` | .` | (_ | (_) \\__ \\   Your version of MaNGOS  \n"
            " |_|  |_\\__,_|_|\\_|\\___|\\___/|___/   could not be detected   \n"
            " __________________________________________________________\n"
            "\n"
            " Website/Forum/Wiki/Issue Tracker: https://www.getmangos.eu\n"
            " __________________________________________________________\n"
            "\n");
        break;
    }
}

// Used by Playerbot

// Function to perform a case-insensitive search of str2 in str1
char* strstri(const std::string& str1, const std::string& str2)
{
    // Convert both strings to lowercase for case-insensitive comparison
    std::string lowerStr1 = str1;
    std::string lowerStr2 = str2;
    std::transform(lowerStr1.begin(), lowerStr1.end(), lowerStr1.begin(), ::tolower);
    std::transform(lowerStr2.begin(), lowerStr2.end(), lowerStr2.begin(), ::tolower);

    // Find the first occurrence of lowerStr2 in lowerStr1
    size_t pos = lowerStr1.find(lowerStr2);
    if (pos != std::string::npos)
    {
        // Return the pointer to the first occurrence in the original string
        return (char*)str1.c_str() + pos;
    }
    return nullptr;
}
