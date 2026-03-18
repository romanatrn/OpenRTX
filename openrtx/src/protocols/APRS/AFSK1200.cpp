#include "protocols/APRS/AFSK1200.hpp"
#include "protocols/APRS/AX25.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>

namespace APRS
{

static constexpr float PI_F = 3.1415926535f;
static constexpr float MARK_FREQ = 1200.0f;
static constexpr float SPACE_FREQ = 2200.0f;
static constexpr float AMP = 12000.0f;
static constexpr float SAMPLES_PER_SYMBOL = 8.0f;

static void appendBits(std::vector< uint8_t >& bits, uint8_t value)
{
    for(uint8_t i = 0; i < 8; ++i)
        bits.push_back((value >> i) & 0x01);
}

static std::vector< uint8_t > encodeHdlcBits(const uint8_t *frame,
                                             size_t length,
                                             uint16_t preambleFlags,
                                             uint16_t tailFlags)
{
    std::vector< uint8_t > bits;
    bits.reserve((preambleFlags + tailFlags + length + 1) * 8);

    for(uint16_t i = 0; i < preambleFlags; ++i)
        appendBits(bits, 0x7E);

    uint8_t oneCount = 0;
    for(size_t i = 0; i < length; ++i)
    {
        for(uint8_t bit = 0; bit < 8; ++bit)
        {
            const uint8_t value = (frame[i] >> bit) & 0x01;
            bits.push_back(value);

            if(value != 0)
            {
                oneCount++;
                if(oneCount == 5)
                {
                    bits.push_back(0);
                    oneCount = 0;
                }
            }
            else
            {
                oneCount = 0;
            }
        }
    }

    for(uint16_t i = 0; i < tailFlags; ++i)
        appendBits(bits, 0x7E);

    return bits;
}

static std::vector< int16_t > renderSamples(const std::vector< uint8_t >& bits)
{
    std::vector< int16_t > samples;
    samples.reserve(bits.size() * static_cast< size_t >(SAMPLES_PER_SYMBOL));

    bool mark = true;
    float phase = 0.0f;

    for(uint8_t bit : bits)
    {
        if(bit == 0)
            mark = !mark;

        const float freq = mark ? MARK_FREQ : SPACE_FREQ;
        const float phaseStep = (2.0f * PI_F * freq) / static_cast< float >(AFSK1200Modulator::SAMPLE_RATE);

        for(uint8_t i = 0; i < static_cast< uint8_t >(SAMPLES_PER_SYMBOL); ++i)
        {
            samples.push_back(static_cast< int16_t >(std::sin(phase) * AMP));
            phase += phaseStep;
            if(phase > (2.0f * PI_F))
                phase -= (2.0f * PI_F);
        }
    }

    return samples;
}

AFSK1200Modulator::AFSK1200Modulator()
{
}

AFSK1200Modulator::~AFSK1200Modulator()
{
    terminate();
}

void AFSK1200Modulator::init()
{
    dmaBuffer.resize(2 * HALF_BUFFER, 0);
}

void AFSK1200Modulator::terminate()
{
    stop();
    dmaBuffer.clear();
}

bool AFSK1200Modulator::start()
{
    if(running)
        return true;

    outPath = audioPath_request(SOURCE_MCU, SINK_RTX, PRIO_TX);
    if(outPath < 0)
        return false;

    outStream = audioStream_start(outPath,
                                  dmaBuffer.data(),
                                  dmaBuffer.size(),
                                  SAMPLE_RATE,
                                  STREAM_OUTPUT | BUF_CIRC_DOUBLE);
    if(outStream < 0)
    {
        audioPath_release(outPath);
        outPath = -1;
        return false;
    }

    running = true;
    return true;
}

void AFSK1200Modulator::stop()
{
    if(running == false)
        return;

    audioStream_terminate(outStream);
    audioPath_release(outPath);
    outStream = -1;
    outPath = -1;
    running = false;
}

bool AFSK1200Modulator::sendFrame(const uint8_t *frame,
                                  size_t length,
                                  uint16_t preambleFlags,
                                  uint16_t tailFlags)
{
    if((frame == nullptr) || (length == 0) || (running == false))
        return false;

    const std::vector< uint8_t > bits = encodeHdlcBits(frame, length, preambleFlags, tailFlags);
    const std::vector< int16_t > samples = renderSamples(bits);
    size_t samplePos = 0;
    stream_sample_t *idle = outputStream_getIdleBuffer(outStream);

    while((idle != nullptr) && (samplePos < samples.size()))
    {
        const size_t chunk = std::min(HALF_BUFFER, samples.size() - samplePos);
        std::memcpy(idle, samples.data() + samplePos, chunk * sizeof(stream_sample_t));
        if(chunk < HALF_BUFFER)
            std::memset(idle + chunk, 0, (HALF_BUFFER - chunk) * sizeof(stream_sample_t));

        samplePos += chunk;
        if(outputStream_sync(outStream, true) == false)
            return false;

        idle = outputStream_getIdleBuffer(outStream);
    }

    if(idle != nullptr)
    {
        std::memset(idle, 0, HALF_BUFFER * sizeof(stream_sample_t));
        outputStream_sync(outStream, true);
    }

    audioStream_stop(outStream);
    audioPath_release(outPath);
    outStream = -1;
    outPath = -1;
    running = false;
    return true;
}

AFSK1200Demodulator::AFSK1200Demodulator()
{
}

AFSK1200Demodulator::~AFSK1200Demodulator()
{
    terminate();
}

void AFSK1200Demodulator::init()
{
    sampleBuffer.resize(2 * SAMPLE_BUFFER, 0);
    resetDecoder();
}

void AFSK1200Demodulator::terminate()
{
    stop();
    sampleBuffer.clear();
    readyFrames.clear();
}

void AFSK1200Demodulator::start()
{
    if(running)
        return;

    inputPath = audioPath_request(SOURCE_RTX, SINK_MCU, PRIO_RX);
    if(inputPath < 0)
        return;

    inputStream = audioStream_start(inputPath,
                                    sampleBuffer.data(),
                                    sampleBuffer.size(),
                                    SAMPLE_RATE,
                                    STREAM_INPUT | BUF_CIRC_DOUBLE);
    if(inputStream < 0)
    {
        audioPath_release(inputPath);
        inputPath = -1;
        return;
    }

    running = true;
    resetDecoder();
}

void AFSK1200Demodulator::stop()
{
    if(running == false)
        return;

    audioStream_terminate(inputStream);
    audioPath_release(inputPath);
    inputStream = -1;
    inputPath = -1;
    running = false;
    resetDecoder();
}

bool AFSK1200Demodulator::update()
{
    if(running == false)
        return false;

    dataBlock_t block = inputStream_getData(inputStream);
    if(block.data == nullptr)
        return false;

    const size_t framesBefore = readyFrames.size();
    for(size_t i = 0; i < block.len; ++i)
        processSample(block.data[i]);

    return readyFrames.size() != framesBefore;
}

bool AFSK1200Demodulator::hasFrame() const
{
    return readyFrames.empty() == false;
}

std::vector< uint8_t > AFSK1200Demodulator::popFrame()
{
    if(readyFrames.empty())
        return {};

    std::vector< uint8_t > frame = readyFrames.front();
    readyFrames.erase(readyFrames.begin());
    return frame;
}

void AFSK1200Demodulator::resetDecoder()
{
    readyFrames.clear();
    frameBytes.clear();
    haveTone = false;
    currentTone = true;
    skipStuffZero = false;
    inFrame = false;
    toneRun = 0;
    sampleCount = 0;
    shiftReg = 0;
    currentByte = 0;
    currentBit = 0;
    oneCount = 0;
    std::memset(window, 0, sizeof(window));
    windowPos = 0;
}

static float toneEnergy(const int16_t *window, float freq)
{
    float iAcc = 0.0f;
    float qAcc = 0.0f;

    for(size_t i = 0; i < 8; ++i)
    {
        const float phase = (2.0f * PI_F * freq * static_cast< float >(i))
                          / static_cast< float >(AFSK1200Demodulator::SAMPLE_RATE);
        iAcc += static_cast< float >(window[i]) * std::cos(phase);
        qAcc += static_cast< float >(window[i]) * std::sin(phase);
    }

    return (iAcc * iAcc) + (qAcc * qAcc);
}

void AFSK1200Demodulator::processSample(int16_t sample)
{
    window[windowPos] = sample;
    windowPos = (windowPos + 1) % 8;
    sampleCount++;

    if(sampleCount < 8)
        return;

    int16_t ordered[8] = {0};
    for(size_t i = 0; i < 8; ++i)
        ordered[i] = window[(windowPos + i) % 8];

    const float markEnergy = toneEnergy(ordered, MARK_FREQ);
    const float spaceEnergy = toneEnergy(ordered, SPACE_FREQ);
    const bool tone = spaceEnergy > markEnergy;

    if(haveTone == false)
    {
        haveTone = true;
        currentTone = tone;
        toneRun = 1;
        return;
    }

    toneRun++;
    if(tone != currentTone)
    {
        emitToneTransition(toneRun - 1);
        currentTone = tone;
        toneRun = 1;
    }
}

void AFSK1200Demodulator::emitToneTransition(uint32_t runLength)
{
    uint32_t symbols = (runLength + 4) / 8;
    if(symbols == 0)
        symbols = 1;
    if(symbols > 8)
        symbols = 8;

    for(uint32_t i = 1; i < symbols; ++i)
        emitRawBit(1);
    emitRawBit(0);
}

void AFSK1200Demodulator::emitRawBit(uint8_t bit)
{
    shiftReg = static_cast< uint8_t >((shiftReg >> 1) | (bit << 7));
    if(shiftReg == 0x7E)
    {
        if(inFrame && (frameBytes.size() >= 18))
            pushDecodedFrame();

        frameBytes.clear();
        currentByte = 0;
        currentBit = 0;
        oneCount = 0;
        skipStuffZero = false;
        inFrame = true;
        return;
    }

    if(inFrame == false)
        return;

    if(skipStuffZero)
    {
        skipStuffZero = false;
        if(bit == 0)
            return;

        inFrame = false;
        frameBytes.clear();
        currentByte = 0;
        currentBit = 0;
        oneCount = 0;
        return;
    }

    if(bit != 0)
        oneCount++;
    else
        oneCount = 0;

    currentByte |= static_cast< uint8_t >(bit << currentBit);
    currentBit++;
    if(currentBit >= 8)
    {
        frameBytes.push_back(currentByte);
        currentByte = 0;
        currentBit = 0;
    }

    if(oneCount == 5)
        skipStuffZero = true;
}

void AFSK1200Demodulator::pushDecodedFrame()
{
    if(validateFrame(frameBytes.data(), frameBytes.size()))
        readyFrames.push_back(frameBytes);
}

}
