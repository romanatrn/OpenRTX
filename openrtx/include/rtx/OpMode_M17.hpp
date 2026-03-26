/*
 * SPDX-FileCopyrightText: Copyright 2020-2026 OpenRTX Contributors
 * 
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef OPMODE_M17_H
#define OPMODE_M17_H

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include "protocols/M17/M17FrameDecoder.hpp"
#include "protocols/M17/M17FrameEncoder.hpp"
#include "protocols/M17/M17Demodulator.hpp"
#include "protocols/M17/M17Modulator.hpp"
#include "protocols/M17/M17Crypto.hpp"
#include "protocols/M17/MetaText.hpp"
#include "core/audio_path.h"
#include "core/settings.h"
#include "OpMode.hpp"

/**
 * Specialisation of the OpMode class for the management of M17 operating mode.
 */
class OpMode_M17 : public OpMode
{
public:

    /**
     * Constructor.
     */
    OpMode_M17();

    /**
     * Destructor.
     */
    ~OpMode_M17();

    /**
     * Enable the operating mode.
     *
     * Application must ensure this function is being called when entering the
     * new operating mode and always before the first call of "update".
     */
    virtual void enable() override;

    /**
     * Disable the operating mode. This function stops the DMA transfers
     * between the baseband, microphone and speakers. It also ensures that
     * the radio, the audio amplifier and the microphone are in OFF state.
     *
     * Application must ensure this function is being called when exiting the
     * current operating mode.
     */
    virtual void disable() override;

    /**
     * Update the internal FSM.
     * Application code has to call this function periodically, to ensure proper
     * functionality.
     *
     * @param status: pointer to the rtxStatus_t structure containing the current
     * RTX status. Internal FSM may change the current value of the opStatus flag.
     * @param newCfg: flag used inform the internal FSM that a new RTX configuration
     * has been applied.
     */
    virtual void update(rtxStatus_t *const status, const bool newCfg) override;

    /**
     * Get the mode identifier corresponding to the OpMode class.
     *
     * @return the corresponding flag from the opmode enum.
     */
    virtual opmode getID() override
    {
        return OPMODE_M17;
    }

    /**
     * Check if RX squelch is open.
     *
     * @return true if RX squelch is open.
     */
    virtual bool rxSquelchOpen() override
    {
        return dataValid;
    }

private:

    /**
     * Function handling the OFF operating state.
     *
     * @param status: pointer to the rtxStatus_t structure containing the
     * current RTX status.
     */
    void offState(rtxStatus_t *const status);

    /**
     * Function handling the RX operating state.
     *
     * @param status: pointer to the rtxStatus_t structure containing the
     * current RTX status.
     */
    void rxState(rtxStatus_t *const status);

    /**
     * Function handling the TX operating state.
     *
     * @param status: pointer to the rtxStatus_t structure containing the
     * current RTX status.
     */
    void txState(rtxStatus_t *const status);

    /**
     * Compare two callsigns in plain text form.
     * The comparison does not take into account the country prefixes (strips
     * the '/' and whatever is in front from all callsigns). It does take into
     * account the dash and whatever is after it. In case the incoming callsign
     * is "ALL" the function returns true.
     *
     * \param localCs plain text callsign from the user
     * \param incomingCs plain text destination callsign
     * \return true if local an incoming callsigns match.
     */
    bool compareCallsigns(const std::string& localCs, const std::string& incomingCs);

    bool resolveEncryptionConfig(uint8_t& encType, uint8_t& encSubType,
                                 uint8_t& keyIndex) const;

    bool loadKeySlot(uint8_t keyIndex, std::array<uint8_t, 16>& key,
                     size_t& keyLen) const;

    void clearTxMeta(M17::meta_t& meta) const;

    bool syncEnabled() const;
    bool syncSender() const;
    bool syncReceiver() const;
    bool syncShouldStartOnPtt() const;
    bool syncTxPending() const;
    void resetSyncSession();
    void prepareSyncSend();
    void prepareSyncReceive(uint8_t sessionId);
    bool buildSyncPacket(M17::payload_t& payload, bool& lastFrame, rtxStatus_t *status);
    bool processSyncPacket(const M17::payload_t& payload, const M17::streamType_t& type,
                           rtxStatus_t *status);
    bool handleSyncAck(uint8_t category, uint16_t index);
    bool handleSyncError(void);
    bool beginSyncObject(uint8_t category, uint16_t index);
    bool commitSyncObject(void);
    void queueSyncAck(uint8_t category, uint16_t index, uint8_t type);
    void queueSyncError(uint8_t category, uint16_t index);
    void finalizeSyncReceive(void);

    enum SyncPhase : uint8_t
    {
        SYNC_IDLE = 0,
        SYNC_SEND_HELLO,
        SYNC_SEND_MANIFEST_START,
        SYNC_SEND_MANIFEST_CHUNK,
        SYNC_SEND_MANIFEST_END,
        SYNC_SEND_OBJECT_START,
        SYNC_SEND_OBJECT_CHUNK,
        SYNC_SEND_OBJECT_END,
        SYNC_SEND_DONE,
        SYNC_WAIT_ACK,
        SYNC_COMPLETE,
        SYNC_ERROR
    };

    // GPS update interval in superframes. Each superframe is 6 LICH frames
    // (~240 ms), so 25 superframes ≈ 6 seconds.
    static constexpr uint16_t GPS_UPDATE_TICKS = 25;
    static constexpr size_t SYNC_OBJECT_MAX = 192;

    bool startRx;                      ///< Flag for RX management.
    bool startTx;                      ///< Flag for TX management.
    bool locked;                       ///< Demodulator locked on data stream.
    bool dataValid;                    ///< Demodulated data is valid
    bool extendedCall;                 ///< Extended callsign data received
    bool invertTxPhase;                ///< TX signal phase inversion setting.
    bool invertRxPhase;                ///< RX signal phase inversion setting.
    pathId rxAudioPath;                ///< Audio path ID for RX
    pathId txAudioPath;                ///< Audio path ID for TX
    M17::M17Modulator    modulator;    ///< M17 modulator.
    M17::M17Demodulator  demodulator;  ///< M17 demodulator.
    M17::M17FrameDecoder decoder;      ///< M17 frame decoder
    M17::M17FrameEncoder encoder;      ///< M17 frame encoder
    uint16_t gpsTimer;                 ///< GPS data transmission interval timer
    uint16_t txFrameNumber;            ///< Current TX payload frame number
    M17::MetaText metaText;            ///< M17 metatext accumulator
    SyncPhase syncPhase;               ///< Radio sync sender state.
    uint8_t syncSessionId;             ///< Active sync session id.
    uint8_t syncTxSeq;                 ///< Sender sequence counter.
    uint8_t syncRxSeq;                 ///< Receiver duplicate filter.
    uint8_t syncCurrentCategory;       ///< Current category being transferred.
    uint8_t syncSelection;             ///< Session selection bitmap.
    uint8_t syncIncludeKeys;           ///< Session key-transfer flag.
    uint16_t syncCurrentIndex;         ///< Current object index in category.
    uint16_t syncCategoryCount;        ///< Object count in current category.
    size_t syncObjectLen;              ///< Current object serialized length.
    size_t syncObjectOffset;           ///< Current object transfer offset.
    std::array<uint8_t, SYNC_OBJECT_MAX> syncObjectBuf; ///< Current object bytes.
    bool syncAckQueued;                ///< Receiver has ACK/ERR queued for TX.
    bool syncAckError;                 ///< Queued response is an error.
    uint8_t syncAckCategory;           ///< Queued ACK category.
    uint16_t syncAckIndex;             ///< Queued ACK object index.
    uint8_t syncAckType;               ///< Queued ACK source packet type.
    uint8_t syncLastTxType;            ///< Sender last transmitted packet type.
    uint8_t syncLastTxCategory;        ///< Sender last transmitted category.
    uint16_t syncLastTxIndex;          ///< Sender last transmitted index.
    settings_t syncSavedSettings;      ///< Receiver rollback snapshot.
    settings_t syncStagedSettings;     ///< Receiver staged settings blob.
    bool syncHasStagedSettings;        ///< Receiver has staged settings.
    bool syncCodeplugReset;            ///< Receiver cleared codeplug for session.
    bool syncReceiveActive;            ///< Receiver accepted current session.
};

#endif /* OPMODE_M17_H */
