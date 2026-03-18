#include "protocols/APRS/AX25.hpp"
#include <algorithm>
#include <cctype>
#include <cstring>

namespace APRS
{

static void trimTrailingSpaces(char *callsign)
{
    for(int i = 5; i >= 0; --i)
    {
        if(callsign[i] != ' ')
            break;
        callsign[i] = '\0';
    }
}

bool parseAddress(const std::string& text, AX25Address& address)
{
    if(text.empty())
        return false;

    address = AX25Address{};

    const size_t dash = text.find('-');
    std::string callsign = text.substr(0, dash);
    if(callsign.empty() || (callsign.size() > 6))
        return false;

    std::transform(callsign.begin(), callsign.end(), callsign.begin(),
                   [](unsigned char c) { return static_cast< char >(std::toupper(c)); });

    for(char c : callsign)
    {
        if((std::isalnum(static_cast< unsigned char >(c)) == 0) && (c != '/'))
            return false;
    }

    std::memcpy(address.callsign, callsign.c_str(), callsign.size());

    if(dash != std::string::npos)
    {
        const std::string ssidText = text.substr(dash + 1);
        if(ssidText.empty())
            return false;

        int ssid = 0;
        for(char c : ssidText)
        {
            if(std::isdigit(static_cast< unsigned char >(c)) == 0)
                return false;
            ssid = (ssid * 10) + (c - '0');
        }

        if((ssid < 0) || (ssid > 15))
            return false;

        address.ssid = static_cast< uint8_t >(ssid);
    }

    return true;
}

static void encodeAddress(const AX25Address& address, bool last, uint8_t *out)
{
    char padded[6] = {' ', ' ', ' ', ' ', ' ', ' '};
    const size_t len = std::min<size_t>(std::strlen(address.callsign), 6);
    std::memcpy(padded, address.callsign, len);

    for(size_t i = 0; i < 6; ++i)
        out[i] = static_cast< uint8_t >(static_cast< uint8_t >(padded[i]) << 1);

    out[6] = static_cast< uint8_t >(((address.ssid & 0x0F) << 1) | 0x60);
    if(address.repeated)
        out[6] |= 0x80;
    if(last)
        out[6] |= 0x01;
}

uint16_t crcCcitt(const uint8_t *data, size_t length)
{
    uint16_t crc = 0xFFFF;

    for(size_t i = 0; i < length; ++i)
    {
        crc ^= data[i];
        for(uint8_t bit = 0; bit < 8; ++bit)
        {
            if(crc & 0x0001)
                crc = (crc >> 1) ^ 0x8408;
            else
                crc >>= 1;
        }
    }

    return static_cast< uint16_t >(crc ^ 0xFFFF);
}

bool validateFrame(const uint8_t *frame, size_t length)
{
    if(length < 18)
        return false;

    const uint16_t expected = static_cast< uint16_t >(frame[length - 2])
                            | static_cast< uint16_t >(frame[length - 1] << 8);
    const uint16_t actual = crcCcitt(frame, length - 2);
    return expected == actual;
}

size_t buildUIFrame(const AX25Address& source,
                   const AX25Address& destination,
                   const std::vector< AX25Address >& path,
                   const uint8_t *info,
                   size_t infoLength,
                   uint8_t *out,
                   size_t outSize)
{
    const size_t addressCount = 2 + path.size();
    const size_t frameLength = (addressCount * 7) + 2 + infoLength + 2;
    if((out == nullptr) || (frameLength > outSize))
        return 0;

    size_t offset = 0;
    encodeAddress(destination, false, out + offset);
    offset += 7;
    encodeAddress(source, path.empty(), out + offset);
    offset += 7;

    for(size_t i = 0; i < path.size(); ++i)
    {
        encodeAddress(path[i], i == (path.size() - 1), out + offset);
        offset += 7;
    }

    out[offset++] = 0x03;
    out[offset++] = 0xF0;
    std::memcpy(out + offset, info, infoLength);
    offset += infoLength;

    const uint16_t fcs = crcCcitt(out, offset);
    out[offset++] = static_cast< uint8_t >(fcs & 0xFF);
    out[offset++] = static_cast< uint8_t >(fcs >> 8);
    return offset;
}

static bool decodeAddress(const uint8_t *data, AX25Address& address)
{
    address = AX25Address{};
    for(size_t i = 0; i < 6; ++i)
        address.callsign[i] = static_cast< char >(data[i] >> 1);

    trimTrailingSpaces(address.callsign);
    address.ssid = (data[6] >> 1) & 0x0F;
    address.repeated = (data[6] & 0x80) != 0;
    return true;
}

bool decodeUIFrame(const uint8_t *frame,
                   size_t length,
                   AX25Address& source,
                   AX25Address& destination,
                   std::vector< AX25Address >& path,
                   std::string& infoText)
{
    if((length < 18) || (validateFrame(frame, length) == false))
        return false;

    path.clear();
    decodeAddress(frame, destination);
    decodeAddress(frame + 7, source);

    size_t offset = 14;
    while((offset + 7) <= length)
    {
        const bool last = (frame[offset - 1] & 0x01) != 0;
        if(last)
            break;

        AX25Address hop;
        decodeAddress(frame + offset, hop);
        path.push_back(hop);
        offset += 7;
    }

    if((offset + 4) > length)
        return false;
    if((frame[offset] != 0x03) || (frame[offset + 1] != 0xF0))
        return false;

    offset += 2;
    infoText.assign(reinterpret_cast< const char * >(frame + offset),
                    reinterpret_cast< const char * >(frame + length - 2));
    return true;
}

}
