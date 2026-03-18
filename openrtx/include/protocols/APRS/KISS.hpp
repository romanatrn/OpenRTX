#ifndef APRS_KISS_H
#define APRS_KISS_H

#include <stddef.h>
#include <stdint.h>
#include <vector>

namespace APRS
{

class KISS
{
public:
    void poll();
    bool hasPendingFrame() const;
    std::vector< uint8_t > popPendingFrame();
    void sendFrame(const uint8_t *frame, size_t length);

private:
    std::vector< uint8_t > rxBuffer;
    std::vector< std::vector< uint8_t > > pendingFrames;
    bool escaped = false;
    bool inFrame = false;
};

}

#endif
