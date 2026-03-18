#include "interfaces/platform.h"
#include "interfaces/radio.h"
#include "rtx/OpMode_APRS.hpp"
#include "core/state.h"
#include "protocols/APRS/APRSFrame.hpp"
#include "protocols/APRS/AX25.hpp"

static constexpr size_t APRS_MAX_FRAME = 330;

OpMode_APRS::OpMode_APRS()
{
}

OpMode_APRS::~OpMode_APRS()
{
    disable();
}

void OpMode_APRS::enable()
{
    modulator.init();
    demodulator.init();
    startRx = true;
    txPending = false;
    rxActivity = false;
    pendingFrame.clear();
}

void OpMode_APRS::disable()
{
    demodulator.stop();
    demodulator.terminate();
    modulator.stop();
    modulator.terminate();
    radio_disableRtx();
    platform_ledOff(GREEN);
    platform_ledOff(RED);
    startRx = false;
    txPending = false;
    rxActivity = false;
    pendingFrame.clear();
}

bool OpMode_APRS::rxSquelchOpen()
{
    return rxActivity;
}

bool OpMode_APRS::shouldBeacon() const
{
    if((state.settings.aprs_enabled == false) || (state.settings.aprs_auto_beacon == false))
        return false;
    if(state.settings.aprs_interval == 0)
        return false;
    if(APRS::hasValidPosition(state.gps_data) == false)
        return false;

    const long long intervalMs = static_cast< long long >(state.settings.aprs_interval) * 60LL * 1000LL;
    return (getTick() - lastBeaconTick) >= intervalMs;
}

static bool consumeManualBeaconRequest()
{
    if(state.aprs_send_beacon == false)
        return false;

    state.aprs_send_beacon = false;
    return true;
}

bool OpMode_APRS::queuePositionBeacon()
{
    uint8_t frame[APRS_MAX_FRAME] = {0};
    APRS::PositionSettings settings;
    settings.callsign = state.settings.callsign;
    settings.ssid = state.settings.aprs_ssid;
    settings.path = state.settings.aprs_path;
    settings.comment = state.settings.aprs_comment;
    settings.symbolTable = state.settings.aprs_symbol_table;
    settings.symbolCode = state.settings.aprs_symbol_code;

    const size_t length = APRS::buildPositionFrame(state.gps_data, settings, frame, sizeof(frame));
    if(length == 0)
        return false;

    pendingFrame.assign(frame, frame + length);
    txPending = true;
    return true;
}

void OpMode_APRS::serviceReceive(rtxStatus_t *const status)
{
    rxActivity = demodulator.update();

    while(demodulator.hasFrame())
    {
        const std::vector< uint8_t > frame = demodulator.popFrame();
        if(state.settings.aprs_kiss_enabled)
            kiss.sendFrame(frame.data(), frame.size() - 2);
    }

    kiss.poll();
    if((state.settings.aprs_kiss_enabled) && kiss.hasPendingFrame())
    {
        pendingFrame = kiss.popPendingFrame();
        txPending = pendingFrame.empty() == false;
    }

    const bool manualBeacon = consumeManualBeaconRequest();

    if(txPending || manualBeacon || shouldBeacon() || platform_getPttStatus())
    {
        if((txPending == false) && (queuePositionBeacon() == false))
            return;

        demodulator.stop();
        radio_disableRtx();
        status->opStatus = OFF;
        startRx = false;
    }
}

void OpMode_APRS::serviceTransmit(rtxStatus_t *const status)
{
    if(txPending == false)
    {
        status->opStatus = OFF;
        startRx = true;
        return;
    }

    radio_enableTx();
    if(APRS::validateFrame(pendingFrame.data(), pendingFrame.size()) == false)
    {
        const uint16_t fcs = APRS::crcCcitt(pendingFrame.data(), pendingFrame.size());
        pendingFrame.push_back(static_cast< uint8_t >(fcs & 0xFF));
        pendingFrame.push_back(static_cast< uint8_t >(fcs >> 8));
    }

    if(modulator.start())
        modulator.sendFrame(pendingFrame.data(), pendingFrame.size());

    modulator.stop();
    radio_disableRtx();
    pendingFrame.clear();
    txPending = false;
    lastBeaconTick = getTick();
    status->opStatus = OFF;
    startRx = true;
}

void OpMode_APRS::setLeds(const rtxStatus_t *status) const
{
    switch(status->opStatus)
    {
        case RX:
            platform_ledOn(GREEN);
            platform_ledOff(RED);
            break;

        case TX:
            platform_ledOff(GREEN);
            platform_ledOn(RED);
            break;

        default:
            platform_ledOff(GREEN);
            platform_ledOff(RED);
            break;
    }
}

void OpMode_APRS::update(rtxStatus_t *const status, const bool newCfg)
{
    (void) newCfg;

    kiss.poll();

    if((status->opStatus == OFF) && startRx)
    {
        radio_enableRx();
        demodulator.start();
        status->opStatus = RX;
    }

    if(status->opStatus == RX)
    {
        serviceReceive(status);
        if((status->opStatus == OFF) && txPending)
            status->opStatus = TX;
    }
    else if(status->opStatus == TX)
    {
        serviceTransmit(status);
    }

    if((status->opStatus == OFF) && startRx)
        rxActivity = false;

    setLeds(status);
    sleepFor(0u, 20u);
}
