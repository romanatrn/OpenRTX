#ifndef APRS_AX25_H
#define APRS_AX25_H

#include <stddef.h>
#include <stdint.h>
#include <string>
#include <vector>

namespace APRS
{

struct AX25Address
{
    char callsign[7] = {0};
    uint8_t ssid = 0;
    bool repeated = false;
};

bool parseAddress(const std::string& text, AX25Address& address);
uint16_t crcCcitt(const uint8_t *data, size_t length);
bool validateFrame(const uint8_t *frame, size_t length);

size_t buildUIFrame(const AX25Address& source,
                   const AX25Address& destination,
                   const std::vector< AX25Address >& path,
                   const uint8_t *info,
                   size_t infoLength,
                   uint8_t *out,
                   size_t outSize);

bool decodeUIFrame(const uint8_t *frame,
                   size_t length,
                   AX25Address& source,
                   AX25Address& destination,
                   std::vector< AX25Address >& path,
                   std::string& infoText);

}

#endif
