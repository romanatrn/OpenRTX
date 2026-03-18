#include "protocols/APRS/KISS.hpp"

#if defined(STM32F405xx) || defined(MK22FN512xxx12)
#include "drivers/usb_vcom.h"
#define APRS_HAS_VCOM 1
#else
#define APRS_HAS_VCOM 0
#endif

namespace APRS
{

static constexpr uint8_t FEND  = 0xC0;
static constexpr uint8_t FESC  = 0xDB;
static constexpr uint8_t TFEND = 0xDC;
static constexpr uint8_t TFESC = 0xDD;

void KISS::poll()
{
#if APRS_HAS_VCOM
    uint8_t buffer[64] = {0};
    const ssize_t received = vcom_readBlock(buffer, sizeof(buffer));
    for(ssize_t i = 0; i < received; ++i)
    {
        uint8_t byte = buffer[i];

        if(byte == FEND)
        {
            if(inFrame && (rxBuffer.size() > 1) && ((rxBuffer[0] & 0x0F) == 0x00))
            {
                pendingFrames.emplace_back(rxBuffer.begin() + 1, rxBuffer.end());
            }

            rxBuffer.clear();
            escaped = false;
            inFrame = true;
            continue;
        }

        if(inFrame == false)
            continue;

        if(escaped)
        {
            if(byte == TFEND)
                byte = FEND;
            else if(byte == TFESC)
                byte = FESC;
            escaped = false;
        }
        else if(byte == FESC)
        {
            escaped = true;
            continue;
        }

        rxBuffer.push_back(byte);
    }
#endif
}

bool KISS::hasPendingFrame() const
{
    return pendingFrames.empty() == false;
}

std::vector< uint8_t > KISS::popPendingFrame()
{
    if(pendingFrames.empty())
        return {};

    std::vector< uint8_t > frame = pendingFrames.front();
    pendingFrames.erase(pendingFrames.begin());
    return frame;
}

void KISS::sendFrame(const uint8_t *frame, size_t length)
{
#if APRS_HAS_VCOM
    std::vector< uint8_t > encoded;
    encoded.reserve(length + 4);
    encoded.push_back(FEND);
    encoded.push_back(0x00);

    for(size_t i = 0; i < length; ++i)
    {
        const uint8_t byte = frame[i];
        if(byte == FEND)
        {
            encoded.push_back(FESC);
            encoded.push_back(TFEND);
        }
        else if(byte == FESC)
        {
            encoded.push_back(FESC);
            encoded.push_back(TFESC);
        }
        else
        {
            encoded.push_back(byte);
        }
    }

    encoded.push_back(FEND);
    vcom_writeBlock(encoded.data(), encoded.size());
#else
    (void) frame;
    (void) length;
#endif
}

}
