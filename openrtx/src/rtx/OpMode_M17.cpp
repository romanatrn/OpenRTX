/*
 * SPDX-FileCopyrightText: Copyright 2020-2026 OpenRTX Contributors
 * 
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "interfaces/platform.h"
#include "interfaces/delays.h"
#include "interfaces/audio.h"
#include "interfaces/radio.h"
#include "core/crc.h"
#include "core/m17_sync.h"
#include "protocols/M17/M17Datatypes.hpp"
#include "rtx/OpMode_M17.hpp"
#include "core/audio_codec.h"
#include <errno.h>
#include "core/gps.h"
#include "core/state.h"
#include "core/utils.h"
#include "peripherals/rng.h"
#include "rtx/rtx.h"

#ifdef PLATFORM_MOD17
#include "calibration/calibInfo_Mod17.h"
#include "interfaces/platform.h"

extern mod17Calib_t mod17CalData;
#endif

using namespace std;
using namespace M17;

namespace
{
enum SyncPacketType : uint8_t
{
    SYNC_PKT_HELLO = 1,
    SYNC_PKT_ACK   = 2,
    SYNC_PKT_START = 3,
    SYNC_PKT_CHUNK = 4,
    SYNC_PKT_END   = 5,
    SYNC_PKT_DONE  = 6,
    SYNC_PKT_ERROR = 7
};

struct __attribute__((packed)) SyncPacket
{
    uint8_t  type;
    uint8_t  session;
    uint8_t  seq;
    uint8_t  category;
    uint16_t index;
    uint8_t  len;
    uint8_t  data[7];
    uint16_t crc;
};

static void syncPacketFinalize(SyncPacket& pkt)
{
    pkt.crc = crc_ccitt(&pkt, sizeof(pkt) - sizeof(pkt.crc));
}

static bool syncPacketValid(const payload_t& payload, SyncPacket& pkt)
{
    memcpy(&pkt, payload.data(), sizeof(pkt));
    return pkt.crc == crc_ccitt(&pkt, sizeof(pkt) - sizeof(pkt.crc));
}

static bool syncCategorySelected(uint8_t selection, uint8_t category)
{
    switch(category)
    {
        case M17_SYNC_CAT_SETTINGS: return (selection & M17_SYNC_SETTINGS) != 0U;
        case M17_SYNC_CAT_CONTACT:  return (selection & M17_SYNC_CONTACTS) != 0U;
        case M17_SYNC_CAT_CHANNEL:  return (selection & M17_SYNC_CHANNELS) != 0U;
        case M17_SYNC_CAT_ZONE:     return (selection & M17_SYNC_ZONES) != 0U;
        default:                    return false;
    }
}
}

OpMode_M17::OpMode_M17() : startRx(false), startTx(false), locked(false),
                           dataValid(false), extendedCall(false),
                           invertTxPhase(false), invertRxPhase(false),
                           gpsTimer(0), txFrameNumber(0), syncPhase(SYNC_IDLE),
                           syncSessionId(0), syncTxSeq(0), syncRxSeq(0xFF),
                           syncCurrentCategory(0), syncSelection(0),
                           syncIncludeKeys(0), syncCurrentIndex(0),
                           syncCategoryCount(0), syncObjectLen(0),
                           syncObjectOffset(0), syncAckQueued(false),
                           syncAckError(false), syncAckCategory(0),
                           syncAckIndex(0), syncAckType(0), syncLastTxType(0),
                           syncLastTxCategory(0), syncLastTxIndex(0),
                           syncHasStagedSettings(false), syncCodeplugReset(false),
                           syncReceiveActive(false)
{

}

OpMode_M17::~OpMode_M17()
{
    disable();
}

void OpMode_M17::enable()
{
    codec_init();
    modulator.init();
    demodulator.init();
    locked       = false;
    dataValid    = false;
    extendedCall = false;
    txFrameNumber = 0;
    startRx      = true;
    startTx      = false;
    resetSyncSession();
}

void OpMode_M17::disable()
{
    startRx = false;
    startTx = false;
    platform_ledOff(GREEN);
    platform_ledOff(RED);
    audioPath_release(rxAudioPath);
    audioPath_release(txAudioPath);
    codec_terminate();
    resetSyncSession();
    radio_disableRtx();
    modulator.terminate();
    demodulator.terminate();
}

void OpMode_M17::update(rtxStatus_t *const status, const bool newCfg)
{
    (void) newCfg;
    #if defined(PLATFORM_MD3x0) || defined(PLATFORM_MDUV3x0)
    //
    // Invert TX phase for all MDx models.
    // Invert RX phase for MD-3x0 VHF and MD-UV3x0 radios.
    //
    const hwInfo_t* hwinfo = platform_getHwInfo();
    invertTxPhase = true;
    if(hwinfo->vhf_band == 1)
        invertRxPhase = true;
    else
        invertRxPhase = false;
    #elif defined(PLATFORM_MOD17)
    //
    // Get phase inversion settings from calibration.
    //
    invertTxPhase = (mod17CalData.bb_tx_invert == 1) ? true : false;
    invertRxPhase = (mod17CalData.bb_rx_invert == 1) ? true : false;
    #elif defined(PLATFORM_CS7000) || defined(PLATFORM_CS7000P)
    invertTxPhase = true;
    #elif defined(PLATFORM_DM1701)
    invertTxPhase = true;
    invertRxPhase = true;
    #endif

    // Main FSM logic
    switch(status->opStatus)
    {
        case OFF:
            offState(status);
            break;

        case RX:
            rxState(status);
            break;

        case TX:
            txState(status);
            break;

        default:
            break;
    }

    status->M17_sync_role = state.settings.m17_sync_role;
    status->M17_sync_category = syncCurrentCategory;
    status->M17_sync_index = syncCurrentIndex;
    status->M17_sync_count = syncCategoryCount;
    status->M17_sync_active = (syncPhase != SYNC_IDLE)
                           && (syncPhase != SYNC_COMPLETE)
                           && (syncPhase != SYNC_ERROR || syncReceiveActive);
    status->M17_sync_error = (syncPhase == SYNC_ERROR) || syncAckError;

    // Led control logic
    switch(status->opStatus)
    {
        case RX:

            if(dataValid)
                platform_ledOn(GREEN);
            else
                platform_ledOff(GREEN);
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

void OpMode_M17::offState(rtxStatus_t *const status)
{
    radio_disableRtx();

    if(syncEnabled() == false)
        codec_stop(txAudioPath);

    audioPath_release(txAudioPath);

    if(syncTxPending())
    {
        startTx = true;
        status->opStatus = TX;
        return;
    }

    if(startRx)
    {
        status->opStatus = RX;
        return;
    }

    if(platform_getPttStatus() && (status->txDisable == 0))
    {
        if(syncShouldStartOnPtt())
            prepareSyncSend();

        startTx = true;
        status->opStatus = TX;
        return;
    }

    // Sleep for 30ms if there is nothing else to do in order to prevent the
    // rtx thread looping endlessly and locking up all the other tasks
    sleepFor(0, 30);
}

void OpMode_M17::rxState(rtxStatus_t *const status)
{
    if(startRx)
    {
        demodulator.startBasebandSampling();

        radio_enableRx();

        startRx = false;
    }

    bool newData = demodulator.update(invertRxPhase);
    bool lock    = demodulator.isLocked();

    // Reset frame decoder when transitioning from unlocked to locked state.
    if((lock == true) && (locked == false))
    {
        decoder.reset();
        locked = lock;
    }

    if(locked)
    {
        // Process new data
        if(newData)
        {
            auto& frame   = demodulator.getFrame();
            auto  type    = decoder.decodeFrame(frame);
            auto  lsf     = decoder.getLsf();
            status->lsfOk = lsf.valid();

            if(status->lsfOk)
            {
                dataValid = true;

                // Retrieve stream source and destination data
                Callsign dst = lsf.getDestination();
                Callsign src = lsf.getSource();
                strncpy(status->M17_dst, dst, 10);
                
                // Copy source callsign (may be overridden for extended callsigns)
                strncpy(status->M17_src, src, 10);

                // Retrieve extended callsign data
                streamType_t streamType = lsf.getType();

                if(streamType.fields.encType == M17_ENCRYPTION_NONE)
                {
                    meta_t& meta = lsf.metadata();

                    switch(streamType.fields.encSubType)
                    {
                        case M17_META_EXTD_CALLSIGN:
                        {
                            extendedCall = true;
                            Callsign exCall1(meta.extended_call_sign.call1);
                            Callsign exCall2(meta.extended_call_sign.call2);

                            // The source callsign only contains the last link when
                            // receiving extended callsign data: store the first
                            // extended callsign in M17_src.
                            strncpy(status->M17_src,  exCall1, 10);
                            strncpy(status->M17_refl, exCall2, 10);
                            strncpy(status->M17_link, src, 10);
                            break;
                        }
                        case M17_META_TEXT:
                        {
                            metaText.addBlock(meta);
                            const char* txt = metaText.getText();
                            if(txt != nullptr)
                                strncpy(status->M17_meta_text, txt, sizeof(status->M17_meta_text) - 1);
                            break;
                        }
                        default:
                            // M17_src already set above
                            break;
                    }
                }
                // M17_src already set above for non-encrypted streams

                // Check CAN on RX, if enabled.
                // If check is disabled, force match to true.
                bool canMatch =  (streamType.fields.CAN == status->rxCan)
                              || (status->canRxEn == false);

                // Check if the destination callsign of the incoming transmission
                // matches with ours
                bool callMatch = (Callsign(status->source_address) == dst)
                               || dst.isSpecial();

                bool syncPacketMode = (streamType.fields.dataMode == M17_DATAMODE_PACKET)
                                   && (streamType.fields.dataType == M17_DATATYPE_DATA);

                if(canMatch && callMatch && syncPacketMode && (type == M17FrameType::PACKET))
                {
                    M17StreamFrame packetFrame = decoder.getPacketFrame();
                    status->M17_src[sizeof(status->M17_src) - 1] = '\0';
                    processSyncPacket(packetFrame.payload(), streamType, status);
                }

                // Open audio path only if CAN and callsign match
                uint8_t pthSts = audioPath_getStatus(rxAudioPath);
                if((pthSts == PATH_CLOSED) && (canMatch == true) && (callMatch == true)
                   && !syncPacketMode)
                {
                    rxAudioPath = audioPath_request(SOURCE_MCU, SINK_SPK, PRIO_RX);
                    pthSts = audioPath_getStatus(rxAudioPath);
                }

                // Extract audio data and sent it to codec
                if((type == M17FrameType::STREAM) && (pthSts == PATH_OPEN))
                {
                    // (re)start codec2 module if not already up
                    if(codec_running() == false)
                        codec_startDecode(rxAudioPath);

                    M17StreamFrame sf = decoder.getStreamFrame();
                    payload_t audio = sf.payload();
                    uint8_t keyIndex = 0;
                    uint8_t cfgEncType = M17_ENCRYPTION_NONE;
                    uint8_t cfgEncSubType = 0;
                    std::array<uint8_t, 16> key;
                    size_t keyLen = 0;
                    bool decryptOk = (streamType.fields.encType == M17_ENCRYPTION_NONE);

                    if((streamType.fields.encType != M17_ENCRYPTION_NONE)
                       && resolveEncryptionConfig(cfgEncType, cfgEncSubType, keyIndex)
                       && loadKeySlot(keyIndex, key, keyLen))
                    {
                        M17Crypto::decrypt(lsf, audio, key, keyLen,
                                           sf.getFrameNumber() & 0x7FFF);
                        decryptOk = true;
                    }

                    if(decryptOk)
                    {
                        codec_pushFrame(audio.data(),     false);
                        codec_pushFrame(audio.data() + 8, false);
                    }
                }
            }
        }
    }

    locked = lock;

    if(platform_getPttStatus())
    {
        demodulator.stopBasebandSampling();
        locked = false;
        status->opStatus = OFF;
    }

    // Force invalidation of LSF data as soon as lock is lost (for whatever cause)
    if(locked == false)
    {
        status->lsfOk = false;
        dataValid     = false;
        extendedCall  = false;
        status->M17_meta_text[0] = '\0';
        status->M17_link[0] = '\0';
        status->M17_refl[0] = '\0';

        metaText.reset();
        codec_stop(rxAudioPath);
        audioPath_release(rxAudioPath);

        if(syncReceiver() && syncReceiveActive)
            startTx = syncAckQueued;
    }
}

void OpMode_M17::txState(rtxStatus_t *const status)
{
    frame_t m17Frame;

    if(syncEnabled() && (syncSender() || syncAckQueued))
    {
        if(startTx)
        {
            startTx = false;

            M17LinkSetupFrame lsf;
            streamType_t type = {};

            lsf.clear();
            lsf.setSource(status->source_address);
            if(syncAckQueued)
                lsf.setDestination(Callsign(status->M17_src));
            else
                lsf.setDestination(Callsign(status->destination_address));

            type.fields.dataMode = M17_DATAMODE_PACKET;
            type.fields.dataType = M17_DATATYPE_DATA;
            type.fields.encType = M17_ENCRYPTION_NONE;
            type.fields.encSubType = 0;
            type.fields.CAN = status->txCan;
            lsf.setType(type);
            clearTxMeta(lsf.metadata());

            encoder.reset();
            encoder.encodeLsf(lsf, m17Frame);

            radio_enableTx();
            modulator.invertPhase(invertTxPhase);
            modulator.start();
            modulator.sendPreamble();
            modulator.sendFrame(m17Frame);
        }

        payload_t syncPayload = {};
        bool lastFrame = true;

        if(buildSyncPacket(syncPayload, lastFrame, status))
        {
            encoder.encodePacketFrame(syncPayload, m17Frame, lastFrame);
            modulator.sendFrame(m17Frame);
        }

        encoder.encodeEotFrame(m17Frame);
        modulator.sendFrame(m17Frame);
        modulator.stop();
        startRx = true;
        status->opStatus = OFF;
        return;
    }

    if(startTx)
    {
        startTx = false;

        M17LinkSetupFrame lsf;
        uint8_t encType = M17_ENCRYPTION_NONE;
        uint8_t encSubType = M17_META_TEXT;
        uint8_t keyIndex = 0;
        std::array<uint8_t, 16> key;
        size_t keyLen = 0;
        bool encryptionActive = resolveEncryptionConfig(encType, encSubType, keyIndex)
                             && (encType != M17_ENCRYPTION_NONE)
                             && loadKeySlot(keyIndex, key, keyLen);

        lsf.clear();
        lsf.setSource(status->source_address);

        Callsign dst(status->destination_address);
        if(!dst.isEmpty())
            lsf.setDestination(dst);

        streamType_t type = {};
        type.fields.dataMode = M17_DATAMODE_STREAM;     // Stream
        type.fields.dataType = M17_DATATYPE_VOICE;      // Voice data
        type.fields.encType = encryptionActive ? encType
                                               : static_cast<uint8_t>(M17_ENCRYPTION_NONE);
        type.fields.encSubType = encryptionActive ? encSubType
                                                  : static_cast<uint8_t>(M17_META_TEXT);
        type.fields.CAN      = status->txCan;           // Channel access number

        lsf.setType(type);
        clearTxMeta(lsf.metadata());
        txFrameNumber = 0;

        if(!encryptionActive && (strlen(state.settings.M17_meta_text) > 0)) {
            metaText.setText(state.settings.M17_meta_text);
            metaText.getNextBlock(lsf.metadata());
        }

        if(encryptionActive && (encType == M17_ENCRYPTION_AES)) {
            M17Crypto::fillAesMeta(lsf.metadata(), static_cast<uint32_t>(getTick()));
            for(uint8_t i = 4; i < 12; i += 4)
            {
                uint32_t rnd = rng_get();
                lsf.metadata().raw_data[i]     = (rnd >> 24) & 0xFF;
                lsf.metadata().raw_data[i + 1] = (rnd >> 16) & 0xFF;
                lsf.metadata().raw_data[i + 2] = (rnd >> 8) & 0xFF;
                lsf.metadata().raw_data[i + 3] = rnd & 0xFF;
            }
            uint16_t ctrHigh = static_cast<uint16_t>(rng_get());
            lsf.metadata().raw_data[12] = (ctrHigh >> 8) & 0xFF;
            lsf.metadata().raw_data[13] = ctrHigh & 0xFF;
        }

        if(!encryptionActive && state.settings.gps_enabled) {
            lsf.setGnssData(&state.gps_data, M17_GNSS_STATION_HANDHELD);
            gpsTimer = 0;
        }

        encoder.reset();
        encoder.encodeLsf(lsf, m17Frame);

        txAudioPath = audioPath_request(SOURCE_MIC, SINK_MCU, PRIO_TX);
        codec_startEncode(txAudioPath);
        radio_enableTx();

        modulator.invertPhase(invertTxPhase);
        modulator.start();
        modulator.sendPreamble();
        modulator.sendFrame(m17Frame);
    }
    payload_t dataFrame;
    bool      lastFrame = false;

    // Wait until there are 16 bytes of compressed speech, then send them
    codec_popFrame(dataFrame.data(),     true);
    codec_popFrame(dataFrame.data() + 8, true);

    if(platform_getPttStatus() == false)
    {
        lastFrame = true;
        startRx   = true;
        status->opStatus = OFF;
    }

    {
        uint8_t encType = M17_ENCRYPTION_NONE;
        uint8_t encSubType = 0;
        uint8_t keyIndex = 0;
        std::array<uint8_t, 16> key;
        size_t keyLen = 0;

        if(resolveEncryptionConfig(encType, encSubType, keyIndex)
           && (encType != M17_ENCRYPTION_NONE)
           && loadKeySlot(keyIndex, key, keyLen))
        {
            auto lsf = encoder.getCurrentLsf();
            M17Crypto::encrypt(lsf, dataFrame, key, keyLen, txFrameNumber);
        }
    }

    encoder.encodeStreamFrame(dataFrame, m17Frame, lastFrame);
    txFrameNumber = (txFrameNumber + 1) & 0x7FFF;
    modulator.sendFrame(m17Frame);

    // After encoding a stream frame the encoder advances its LICH counter.
    // When it wraps back to zero a new superframe begins and the encoder
    // will accept an updated LSF.  Schedule the next meta-text block or
    // GPS update at this boundary so the new data is transmitted during
    // the upcoming superframe.
    if(encoder.superframeBoundary())
    {
        if((encoder.getCurrentLsf().getType().fields.encType == M17_ENCRYPTION_NONE)
           && (strlen(state.settings.M17_meta_text) > 0)) {
            auto lsf = encoder.getCurrentLsf();
            metaText.getNextBlock(lsf.metadata());
            encoder.updateLsfData(lsf);
        }

        if((encoder.getCurrentLsf().getType().fields.encType == M17_ENCRYPTION_NONE)
           && state.settings.gps_enabled) {
            gpsTimer++;

            if(gpsTimer >= GPS_UPDATE_TICKS) {
                auto lsf = encoder.getCurrentLsf();
                lsf.setGnssData(&state.gps_data, M17_GNSS_STATION_HANDHELD);
                encoder.updateLsfData(lsf);
                gpsTimer = 0;
            }
        }
    }

    if(lastFrame)
    {
        encoder.encodeEotFrame(m17Frame);
        modulator.sendFrame(m17Frame);
        modulator.stop();
    }
}

bool OpMode_M17::compareCallsigns(const std::string& localCs,
                                  const std::string& incomingCs)
{
    if((incomingCs == "ALL") || (incomingCs == "INFO") || (incomingCs == "ECHO"))
        return true;

    std::string truncatedLocal(localCs);
    std::string truncatedIncoming(incomingCs);

    int slashPos = localCs.find_first_of('/');
    if(slashPos <= 2)
        truncatedLocal = localCs.substr(slashPos + 1);

    slashPos = incomingCs.find_first_of('/');
    if(slashPos <= 2)
        truncatedIncoming = incomingCs.substr(slashPos + 1);

    if(truncatedLocal == truncatedIncoming)
        return true;

    return false;
}

bool OpMode_M17::resolveEncryptionConfig(uint8_t& encType, uint8_t& encSubType,
                                         uint8_t& keyIndex) const
{
    encType = state.settings.m17_default_encryption;
    encSubType = state.settings.m17_default_enc_subtype;
    keyIndex = state.settings.m17_default_key_index;

    if((state.tuner_mode != VFO) && (state.channel.mode == OPMODE_M17))
    {
        encType = state.channel.m17.encr;
        encSubType = state.channel.m17.enc_subtype;
        if(state.channel.m17.key_index != 0)
            keyIndex = state.channel.m17.key_index;
    }

    if(encType == PLAIN)
    {
        encType = M17_ENCRYPTION_NONE;
        return true;
    }

    if(encType == SCRAMBLER)
    {
        encType = M17_ENCRYPTION_SCRAMBLER;
        if(encSubType > M17_SCRAMBLING_24BIT)
            encSubType = M17_SCRAMBLING_16BIT;
        return true;
    }

    if(encType == AES)
    {
        encType = M17_ENCRYPTION_AES;
        encSubType = 0;
        return true;
    }

    encType = M17_ENCRYPTION_NONE;
    encSubType = M17_META_TEXT;
    return false;
}

bool OpMode_M17::loadKeySlot(uint8_t keyIndex, std::array<uint8_t, 16>& key,
                             size_t& keyLen) const
{
    key.fill(0);
    keyLen = 0;

    if((keyIndex == 0) || (keyIndex > M17_KEY_SLOTS))
        return false;

    return M17Crypto::parseHexKey(state.settings.m17_keys[keyIndex - 1], key,
                                  keyLen);
}

void OpMode_M17::clearTxMeta(M17::meta_t& meta) const
{
    memset(meta.raw_data, 0, sizeof(meta.raw_data));
}

bool OpMode_M17::syncEnabled() const
{
    return (state.settings.m17_sync_role != M17_SYNC_OFF)
        && (state.settings.m17_sync_flags != 0U);
}

bool OpMode_M17::syncSender() const
{
    return state.settings.m17_sync_role == M17_SYNC_SEND;
}

bool OpMode_M17::syncReceiver() const
{
    return state.settings.m17_sync_role == M17_SYNC_RECEIVE;
}

bool OpMode_M17::syncShouldStartOnPtt() const
{
    return syncSender() && ((syncPhase == SYNC_IDLE) || (syncPhase == SYNC_COMPLETE)
                         || (syncPhase == SYNC_ERROR));
}

bool OpMode_M17::syncTxPending() const
{
    return syncAckQueued || (syncSender() && (syncPhase != SYNC_IDLE)
                          && (syncPhase != SYNC_WAIT_ACK)
                          && (syncPhase != SYNC_COMPLETE)
                          && (syncPhase != SYNC_ERROR));
}

void OpMode_M17::resetSyncSession()
{
    syncPhase = SYNC_IDLE;
    syncSessionId = 0;
    syncTxSeq = 0;
    syncRxSeq = 0xFF;
    syncCurrentCategory = 0;
    syncSelection = 0;
    syncIncludeKeys = 0;
    syncCurrentIndex = 0;
    syncCategoryCount = 0;
    syncObjectLen = 0;
    syncObjectOffset = 0;
    syncObjectBuf.fill(0);
    syncAckQueued = false;
    syncAckError = false;
    syncAckCategory = 0;
    syncAckIndex = 0;
    syncAckType = 0;
    syncLastTxType = 0;
    syncLastTxCategory = 0;
    syncLastTxIndex = 0;
    syncHasStagedSettings = false;
    syncCodeplugReset = false;
    syncReceiveActive = false;
}

void OpMode_M17::prepareSyncSend()
{
    resetSyncSession();
    syncPhase = SYNC_SEND_HELLO;
    syncSessionId = (uint8_t) (rng_get() & 0xFFU);
    if(syncSessionId == 0U)
        syncSessionId = 1U;
    syncSelection = state.settings.m17_sync_flags;
    syncIncludeKeys = state.settings.m17_sync_include_keys ? 1U : 0U;
}

void OpMode_M17::prepareSyncReceive(uint8_t sessionId)
{
    resetSyncSession();
    syncSessionId = sessionId;
    syncReceiveActive = true;
    syncSavedSettings = state.settings;
    syncStagedSettings = state.settings;
}

bool OpMode_M17::beginSyncObject(uint8_t category, uint16_t index)
{
    syncObjectLen = m17SyncSerializeObject(category, index, &state.settings,
                                           syncObjectBuf.data(), syncObjectBuf.size());
    syncObjectOffset = 0U;
    syncCurrentCategory = category;
    syncCurrentIndex = index;
    return syncObjectLen > 0U;
}

bool OpMode_M17::commitSyncObject(void)
{
    if(syncCurrentCategory == M17_SYNC_CAT_MANIFEST)
        return true;

    if((syncCurrentCategory == M17_SYNC_CAT_CONTACT)
       || (syncCurrentCategory == M17_SYNC_CAT_CHANNEL)
       || (syncCurrentCategory == M17_SYNC_CAT_ZONE))
    {
        if((syncCodeplugReset == false)
           && (m17SyncResetSelectedCodeplug(syncSelection) != 0))
            return false;

        syncCodeplugReset = true;
    }

    return m17SyncApplyObject(syncCurrentCategory, syncCurrentIndex,
                              syncObjectBuf.data(), syncObjectOffset,
                              &syncStagedSettings) == 0;
}

void OpMode_M17::queueSyncAck(uint8_t category, uint16_t index, uint8_t type)
{
    syncAckQueued = true;
    syncAckError = false;
    syncAckCategory = category;
    syncAckIndex = index;
    syncAckType = type;
}

void OpMode_M17::queueSyncError(uint8_t category, uint16_t index)
{
    syncAckQueued = true;
    syncAckError = true;
    syncAckCategory = category;
    syncAckIndex = index;
    syncAckType = SYNC_PKT_ERROR;
    syncStagedSettings = syncSavedSettings;
    syncHasStagedSettings = false;
    if(m17SyncJournalPending())
        m17SyncJournalRollback(&state.settings);
    state.rtx_sync_pending = true;
}

void OpMode_M17::finalizeSyncReceive(void)
{
    if(syncHasStagedSettings)
    {
        state.settings = syncStagedSettings;
        state.rtx_sync_pending = true;
    }

    m17SyncJournalCommit();

    syncReceiveActive = false;
}

bool OpMode_M17::handleSyncError(void)
{
    syncPhase = SYNC_ERROR;
    return true;
}

bool OpMode_M17::handleSyncAck(uint8_t category, uint16_t index)
{
    (void) category;
    (void) index;

    if(syncPhase != SYNC_WAIT_ACK)
        return false;

    switch(syncLastTxType)
    {
        case SYNC_PKT_HELLO:
            syncCurrentCategory = M17_SYNC_CAT_MANIFEST;
            syncCurrentIndex = 0U;
            if(!beginSyncObject(syncCurrentCategory, syncCurrentIndex))
                return handleSyncError();
            syncPhase = SYNC_SEND_MANIFEST_START;
            return true;

        case SYNC_PKT_START:
            if(syncLastTxCategory == M17_SYNC_CAT_MANIFEST)
                syncPhase = SYNC_SEND_MANIFEST_CHUNK;
            else
                syncPhase = SYNC_SEND_OBJECT_CHUNK;
            return true;

        case SYNC_PKT_CHUNK:
            if(syncObjectOffset < syncObjectLen)
            {
                syncPhase = (syncLastTxCategory == M17_SYNC_CAT_MANIFEST) ?
                            SYNC_SEND_MANIFEST_CHUNK : SYNC_SEND_OBJECT_CHUNK;
            }
            else
            {
                syncPhase = (syncLastTxCategory == M17_SYNC_CAT_MANIFEST) ?
                            SYNC_SEND_MANIFEST_END : SYNC_SEND_OBJECT_END;
            }
            return true;

        case SYNC_PKT_END:
            break;

        case SYNC_PKT_DONE:
            syncPhase = SYNC_COMPLETE;
            return true;

        default:
            return false;
    }

    uint8_t nextCategory = syncLastTxCategory;
    uint16_t nextIndex = syncLastTxIndex + 1U;

    while(true)
    {
        if(nextCategory == M17_SYNC_CAT_MANIFEST)
            nextCategory = M17_SYNC_CAT_SETTINGS;

        if((nextCategory >= M17_SYNC_CAT_SETTINGS) && (nextCategory <= M17_SYNC_CAT_ZONE)
           && !syncCategorySelected(syncSelection, nextCategory))
        {
            nextCategory++;
            nextIndex = 0U;
            continue;
        }

        if(nextCategory > M17_SYNC_CAT_ZONE)
        {
            syncPhase = SYNC_SEND_DONE;
            return true;
        }

        syncCategoryCount = m17SyncGetObjectCount(nextCategory, &state.settings);
        if(nextIndex < syncCategoryCount)
        {
            if(!beginSyncObject(nextCategory, nextIndex))
                return handleSyncError();

            syncPhase = SYNC_SEND_OBJECT_START;
            return true;
        }

        nextCategory++;
        nextIndex = 0U;
    }
}

bool OpMode_M17::buildSyncPacket(M17::payload_t& payload, bool& lastFrame,
                                 rtxStatus_t *status)
{
    SyncPacket pkt = {};
    size_t remaining = 0U;
    size_t chunkLen = 0U;

    (void) status;
    payload.fill(0);
    lastFrame = true;

    if(syncAckQueued)
    {
        pkt.type = syncAckError ? SYNC_PKT_ERROR : SYNC_PKT_ACK;
        pkt.session = syncSessionId;
        pkt.seq = ++syncTxSeq;
        pkt.category = syncAckCategory;
        pkt.index = syncAckIndex;
        pkt.len = 1U;
        pkt.data[0] = syncAckType;
        syncPacketFinalize(pkt);
        memcpy(payload.data(), &pkt, sizeof(pkt));

        if(syncAckError || (syncAckType == SYNC_PKT_DONE))
            syncReceiveActive = false;

        syncAckQueued = false;
        return true;
    }

    if(!syncSender())
        return false;

    pkt.session = syncSessionId;
    pkt.seq = ++syncTxSeq;

    switch(syncPhase)
    {
        case SYNC_SEND_HELLO:
            pkt.type = SYNC_PKT_HELLO;
            pkt.len = 2U;
            pkt.data[0] = syncSelection;
            pkt.data[1] = syncIncludeKeys;
            break;

        case SYNC_SEND_MANIFEST_START:
        case SYNC_SEND_OBJECT_START:
            pkt.type = SYNC_PKT_START;
            pkt.category = syncCurrentCategory;
            pkt.index = syncCurrentIndex;
            pkt.len = 2U;
            pkt.data[0] = syncObjectLen & 0xFFU;
            pkt.data[1] = (syncObjectLen >> 8) & 0xFFU;
            break;

        case SYNC_SEND_MANIFEST_CHUNK:
        case SYNC_SEND_OBJECT_CHUNK:
            pkt.type = SYNC_PKT_CHUNK;
            pkt.category = syncCurrentCategory;
            pkt.index = syncCurrentIndex;
            remaining = syncObjectLen - syncObjectOffset;
            chunkLen = (remaining > sizeof(pkt.data)) ? sizeof(pkt.data) : remaining;
            pkt.len = chunkLen;
            memcpy(pkt.data, syncObjectBuf.data() + syncObjectOffset, chunkLen);
            syncObjectOffset += chunkLen;
            break;

        case SYNC_SEND_MANIFEST_END:
        case SYNC_SEND_OBJECT_END:
            pkt.type = SYNC_PKT_END;
            pkt.category = syncCurrentCategory;
            pkt.index = syncCurrentIndex;
            pkt.len = 0U;
            break;

        case SYNC_SEND_DONE:
            pkt.type = SYNC_PKT_DONE;
            pkt.len = 0U;
            break;

        default:
            return false;
    }

    syncLastTxType = pkt.type;
    syncLastTxCategory = pkt.category;
    syncLastTxIndex = pkt.index;
    syncPhase = SYNC_WAIT_ACK;

    syncPacketFinalize(pkt);
    memcpy(payload.data(), &pkt, sizeof(pkt));
    return true;
}

bool OpMode_M17::processSyncPacket(const M17::payload_t& payload,
                                   const M17::streamType_t& type,
                                   rtxStatus_t *status)
{
    SyncPacket pkt = {};

    if(syncPacketValid(payload, pkt) == false)
        return false;

    if((type.fields.dataMode != M17_DATAMODE_PACKET)
       || (type.fields.dataType != M17_DATATYPE_DATA))
        return false;

    if(syncSender() && (pkt.session == syncSessionId)
       && ((pkt.type == SYNC_PKT_ACK) || (pkt.type == SYNC_PKT_ERROR)))
    {
        if(pkt.type == SYNC_PKT_ERROR)
            handleSyncError();
        else
            handleSyncAck(pkt.category, pkt.index);

        demodulator.stopBasebandSampling();
        locked = false;
        startTx = syncTxPending();
        status->opStatus = OFF;
        return true;
    }

    if(!syncReceiver())
        return false;

    if(pkt.type == SYNC_PKT_HELLO)
    {
        prepareSyncReceive(pkt.session);
        syncSelection = pkt.data[0];
        syncIncludeKeys = pkt.data[1];
        syncStagedSettings.m17_sync_flags = syncSelection;
        syncStagedSettings.m17_sync_include_keys = syncIncludeKeys ? true : false;

        if(m17SyncJournalBegin(&syncStagedSettings) != 0)
        {
            queueSyncError(0U, 0U);
        }

        strncpy(status->destination_address, status->M17_src, sizeof(status->destination_address));
        if(!syncAckError)
            queueSyncAck(0U, 0U, SYNC_PKT_HELLO);
    }
    else if((syncReceiveActive == false) || (pkt.session != syncSessionId))
    {
        return false;
    }
    else if(pkt.seq == syncRxSeq)
    {
        if(syncAckQueued)
        {
            demodulator.stopBasebandSampling();
            locked = false;
            startTx = true;
            status->opStatus = OFF;
        }
        return true;
    }
    else
    {
        syncRxSeq = pkt.seq;

        switch(pkt.type)
        {
            case SYNC_PKT_START:
                syncCurrentCategory = pkt.category;
                syncCurrentIndex = pkt.index;
                syncObjectLen = pkt.data[0] | (pkt.data[1] << 8);
                syncObjectOffset = 0U;
                if(syncObjectLen > syncObjectBuf.size())
                    queueSyncError(pkt.category, pkt.index);
                else
                    queueSyncAck(pkt.category, pkt.index, SYNC_PKT_START);
                break;

            case SYNC_PKT_CHUNK:
                if((syncObjectOffset + pkt.len) > syncObjectBuf.size())
                {
                    queueSyncError(pkt.category, pkt.index);
                    break;
                }

                memcpy(syncObjectBuf.data() + syncObjectOffset, pkt.data, pkt.len);
                syncObjectOffset += pkt.len;
                queueSyncAck(pkt.category, pkt.index, SYNC_PKT_CHUNK);
                break;

            case SYNC_PKT_END:
                if((syncCurrentCategory != pkt.category) || (syncCurrentIndex != pkt.index)
                   || (syncObjectOffset != syncObjectLen) || !commitSyncObject())
                {
                    queueSyncError(pkt.category, pkt.index);
                    break;
                }

                if(pkt.category == M17_SYNC_CAT_SETTINGS)
                    syncHasStagedSettings = true;

                queueSyncAck(pkt.category, pkt.index, SYNC_PKT_END);
                break;

            case SYNC_PKT_DONE:
                finalizeSyncReceive();
                queueSyncAck(0U, 0U, SYNC_PKT_DONE);
                break;

            default:
                return false;
        }
    }

    demodulator.stopBasebandSampling();
    locked = false;
    startTx = true;
    status->opStatus = OFF;
    return true;
}
