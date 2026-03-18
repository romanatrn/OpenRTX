#include "protocols/APRS/APRSFrame.hpp"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace APRS
{

bool hasValidPosition(const gps_t& gps)
{
    return (gps.fix_quality != FIX_QUALITY_NO_FIX)
        && ((gps.latitude != 0) || (gps.longitude != 0));
}

static std::string formatCoordinate(const int32_t value,
                                    const bool latitude,
                                    const char positive,
                                    const char negative)
{
    const int32_t absValue = std::abs(value);
    const int32_t degrees = absValue / 1000000;
    const int32_t fractional = absValue % 1000000;
    const double minutes = (static_cast< double >(fractional) * 60.0) / 1000000.0;
    const int minuteWhole = static_cast< int >(minutes);
    const int minuteHundredths = static_cast< int >(std::round((minutes - minuteWhole) * 100.0));
    const int degWidth = latitude ? 2 : 3;
    char buffer[16] = {0};

    std::snprintf(buffer, sizeof(buffer), "%0*d%02d.%02d%c",
                  degWidth,
                  static_cast< int >(degrees),
                  minuteWhole,
                  minuteHundredths % 100,
                  (value >= 0) ? positive : negative);
    return std::string(buffer);
}

std::string formatPositionReport(const gps_t& gps, const PositionSettings& settings)
{
    std::string report = "!";
    report += formatCoordinate(gps.latitude, true, 'N', 'S');
    report += settings.symbolTable;
    report += formatCoordinate(gps.longitude, false, 'E', 'W');
    report += settings.symbolCode;

    if((settings.comment != nullptr) && (settings.comment[0] != '\0'))
    {
        report += settings.comment;
    }

    return report;
}

std::vector< AX25Address > parsePath(const char *path)
{
    std::vector< AX25Address > result;
    if((path == nullptr) || (path[0] == '\0'))
        return result;

    const char *start = path;
    while(*start != '\0')
    {
        const char *end = std::strchr(start, ',');
        std::string token;
        if(end == nullptr)
            token.assign(start);
        else
            token.assign(start, end);

        while((token.empty() == false) && (token.front() == ' '))
            token.erase(token.begin());
        while((token.empty() == false) && (token.back() == ' '))
            token.pop_back();

        AX25Address address;
        if(parseAddress(token, address))
            result.push_back(address);

        if(end == nullptr)
            break;
        start = end + 1;
    }

    return result;
}

size_t buildPositionFrame(const gps_t& gps,
                          const PositionSettings& settings,
                          uint8_t *out,
                          size_t outSize)
{
    if((settings.callsign == nullptr) || (settings.callsign[0] == '\0'))
        return 0;
    if(hasValidPosition(gps) == false)
        return 0;

    AX25Address source;
    if(parseAddress(std::string(settings.callsign) + "-" + std::to_string(settings.ssid), source) == false)
    {
        if(parseAddress(settings.callsign, source) == false)
            return 0;
        source.ssid = settings.ssid;
    }

    AX25Address destination;
    if(parseAddress(settings.destination, destination) == false)
        return 0;

    const std::string report = formatPositionReport(gps, settings);
    const std::vector< AX25Address > path = parsePath(settings.path);
    return buildUIFrame(source,
                        destination,
                        path,
                        reinterpret_cast< const uint8_t * >(report.data()),
                        report.size(),
                        out,
                        outSize);
}

}
