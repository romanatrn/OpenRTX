#ifndef APRS_FRAME_H
#define APRS_FRAME_H

#include "core/gps.h"
#include "protocols/APRS/AX25.hpp"
#include <stddef.h>
#include <stdint.h>
#include <string>
#include <vector>

namespace APRS
{

struct PositionSettings
{
    const char *callsign = nullptr;
    uint8_t ssid = 0;
    const char *path = nullptr;
    const char *comment = nullptr;
    char symbolTable = '/';
    char symbolCode = '[';
    const char *destination = "APRTX1";
};

bool hasValidPosition(const gps_t& gps);
std::string formatPositionReport(const gps_t& gps, const PositionSettings& settings);
std::vector< AX25Address > parsePath(const char *path);

size_t buildPositionFrame(const gps_t& gps,
                          const PositionSettings& settings,
                          uint8_t *out,
                          size_t outSize);

}

#endif
