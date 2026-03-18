#ifndef OPMODE_APRS_H
#define OPMODE_APRS_H

#include "OpMode.hpp"
#include "protocols/APRS/AFSK1200.hpp"
#include "protocols/APRS/KISS.hpp"
#include <vector>

class OpMode_APRS : public OpMode
{
public:
    OpMode_APRS();
    ~OpMode_APRS();

    virtual void enable() override;
    virtual void disable() override;
    virtual void update(rtxStatus_t *const status, const bool newCfg) override;
    virtual opmode getID() override
    {
        return OPMODE_APRS;
    }

    virtual bool rxSquelchOpen() override;

private:
    bool shouldBeacon() const;
    bool queuePositionBeacon();
    void serviceReceive(rtxStatus_t *const status);
    void serviceTransmit(rtxStatus_t *const status);
    void setLeds(const rtxStatus_t *status) const;

    bool startRx = true;
    bool txPending = false;
    bool rxActivity = false;
    long long lastBeaconTick = 0;
    std::vector< uint8_t > pendingFrame;
    APRS::AFSK1200Modulator modulator;
    APRS::AFSK1200Demodulator demodulator;
    APRS::KISS kiss;
};

#endif
