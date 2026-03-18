#ifndef APRS_AFSK1200_H
#define APRS_AFSK1200_H

#include "core/audio_path.h"
#include "core/audio_stream.h"
#include <stddef.h>
#include <stdint.h>
#include <vector>

namespace APRS
{

class AFSK1200Modulator
{
public:
    static constexpr uint32_t SAMPLE_RATE = 9600;

    AFSK1200Modulator();
    ~AFSK1200Modulator();

    void init();
    void terminate();
    bool start();
    void stop();
    bool sendFrame(const uint8_t *frame, size_t length, uint16_t preambleFlags = 32, uint16_t tailFlags = 4);

private:
    static constexpr size_t HALF_BUFFER = 960;

    std::vector< int16_t > dmaBuffer;
    pathId outPath = -1;
    streamId outStream = -1;
    bool running = false;
};

class AFSK1200Demodulator
{
public:
    static constexpr uint32_t SAMPLE_RATE = 9600;

    AFSK1200Demodulator();
    ~AFSK1200Demodulator();

    void init();
    void terminate();
    void start();
    void stop();
    bool update();
    bool hasFrame() const;
    std::vector< uint8_t > popFrame();

private:
    static constexpr size_t SAMPLE_BUFFER = 480;

    void resetDecoder();
    void processSample(int16_t sample);
    void emitToneTransition(uint32_t runLength);
    void emitRawBit(uint8_t bit);
    void pushDecodedFrame();

    std::vector< int16_t > sampleBuffer;
    std::vector< uint8_t > frameBytes;
    std::vector< std::vector< uint8_t > > readyFrames;
    pathId inputPath = -1;
    streamId inputStream = -1;
    bool running = false;
    bool haveTone = false;
    bool currentTone = true;
    bool skipStuffZero = false;
    bool inFrame = false;
    uint32_t toneRun = 0;
    uint32_t sampleCount = 0;
    uint8_t shiftReg = 0;
    uint8_t currentByte = 0;
    uint8_t currentBit = 0;
    uint8_t oneCount = 0;
    int16_t window[8] = {0};
    size_t windowPos = 0;
};

}

#endif
