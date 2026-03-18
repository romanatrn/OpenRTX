/*
 * SPDX-FileCopyrightText: Copyright 2020-2026 OpenRTX Contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef M17_CRYPTO_H
#define M17_CRYPTO_H

#ifndef __cplusplus
#error This header is C++ only!
#endif

#include <array>
#include <cstddef>
#include <cstdint>
#include "protocols/M17/M17Datatypes.hpp"

namespace M17
{

class M17LinkSetupFrame;

class M17Crypto
{
public:
    static bool parseHexKey(const char *hex, std::array<uint8_t, 16>& key,
                            size_t& keyLen);

    static void encrypt(const M17LinkSetupFrame& lsf, payload_t& payload,
                        const std::array<uint8_t, 16>& key, size_t keyLen,
                        uint16_t frameNumber);

    static void decrypt(const M17LinkSetupFrame& lsf, payload_t& payload,
                        const std::array<uint8_t, 16>& key, size_t keyLen,
                        uint16_t frameNumber)
    {
        encrypt(lsf, payload, key, keyLen, frameNumber);
    }

    static void fillAesMeta(meta_t& meta, uint32_t timestamp);

private:
    static void applyScrambler(payload_t& payload, uint32_t seed,
                               uint8_t subtype);
    static void applyAesCtr(payload_t& payload, const meta_t& meta,
                            const std::array<uint8_t, 16>& key,
                            uint16_t frameNumber);
};

}

#endif
