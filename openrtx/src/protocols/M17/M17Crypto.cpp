/*
 * SPDX-FileCopyrightText: Copyright 2020-2026 OpenRTX Contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <cstring>
#include "protocols/M17/M17Crypto.hpp"
#include "protocols/M17/M17LinkSetupFrame.hpp"

using namespace M17;

namespace
{

constexpr uint8_t AES_NB = 4;
constexpr uint8_t AES_NK = 4;
constexpr uint8_t AES_NR = 10;

constexpr uint8_t sbox[256] = {
    0x63,0x7c,0x77,0x7b,0xf2,0x6b,0x6f,0xc5,0x30,0x01,0x67,0x2b,0xfe,0xd7,0xab,0x76,
    0xca,0x82,0xc9,0x7d,0xfa,0x59,0x47,0xf0,0xad,0xd4,0xa2,0xaf,0x9c,0xa4,0x72,0xc0,
    0xb7,0xfd,0x93,0x26,0x36,0x3f,0xf7,0xcc,0x34,0xa5,0xe5,0xf1,0x71,0xd8,0x31,0x15,
    0x04,0xc7,0x23,0xc3,0x18,0x96,0x05,0x9a,0x07,0x12,0x80,0xe2,0xeb,0x27,0xb2,0x75,
    0x09,0x83,0x2c,0x1a,0x1b,0x6e,0x5a,0xa0,0x52,0x3b,0xd6,0xb3,0x29,0xe3,0x2f,0x84,
    0x53,0xd1,0x00,0xed,0x20,0xfc,0xb1,0x5b,0x6a,0xcb,0xbe,0x39,0x4a,0x4c,0x58,0xcf,
    0xd0,0xef,0xaa,0xfb,0x43,0x4d,0x33,0x85,0x45,0xf9,0x02,0x7f,0x50,0x3c,0x9f,0xa8,
    0x51,0xa3,0x40,0x8f,0x92,0x9d,0x38,0xf5,0xbc,0xb6,0xda,0x21,0x10,0xff,0xf3,0xd2,
    0xcd,0x0c,0x13,0xec,0x5f,0x97,0x44,0x17,0xc4,0xa7,0x7e,0x3d,0x64,0x5d,0x19,0x73,
    0x60,0x81,0x4f,0xdc,0x22,0x2a,0x90,0x88,0x46,0xee,0xb8,0x14,0xde,0x5e,0x0b,0xdb,
    0xe0,0x32,0x3a,0x0a,0x49,0x06,0x24,0x5c,0xc2,0xd3,0xac,0x62,0x91,0x95,0xe4,0x79,
    0xe7,0xc8,0x37,0x6d,0x8d,0xd5,0x4e,0xa9,0x6c,0x56,0xf4,0xea,0x65,0x7a,0xae,0x08,
    0xba,0x78,0x25,0x2e,0x1c,0xa6,0xb4,0xc6,0xe8,0xdd,0x74,0x1f,0x4b,0xbd,0x8b,0x8a,
    0x70,0x3e,0xb5,0x66,0x48,0x03,0xf6,0x0e,0x61,0x35,0x57,0xb9,0x86,0xc1,0x1d,0x9e,
    0xe1,0xf8,0x98,0x11,0x69,0xd9,0x8e,0x94,0x9b,0x1e,0x87,0xe9,0xce,0x55,0x28,0xdf,
    0x8c,0xa1,0x89,0x0d,0xbf,0xe6,0x42,0x68,0x41,0x99,0x2d,0x0f,0xb0,0x54,0xbb,0x16
};

constexpr uint8_t rcon[11] = { 0x00, 0x01, 0x02, 0x04, 0x08, 0x10,
                               0x20, 0x40, 0x80, 0x1B, 0x36 };

inline uint8_t xtime(uint8_t x)
{
    return static_cast<uint8_t>((x << 1) ^ (((x >> 7) & 1) * 0x1B));
}

void keyExpansion(const uint8_t *key, uint8_t roundKey[176])
{
    memcpy(roundKey, key, 16);

    uint8_t tmp[4];
    uint8_t i = AES_NK;
    while(i < AES_NB * (AES_NR + 1))
    {
        memcpy(tmp, &roundKey[(i - 1) * 4], sizeof(tmp));
        if((i % AES_NK) == 0)
        {
            uint8_t t = tmp[0];
            tmp[0] = sbox[tmp[1]];
            tmp[1] = sbox[tmp[2]];
            tmp[2] = sbox[tmp[3]];
            tmp[3] = sbox[t];
            tmp[0] ^= rcon[i / AES_NK];
        }

        for(uint8_t j = 0; j < 4; ++j)
            roundKey[i * 4 + j] = roundKey[(i - AES_NK) * 4 + j] ^ tmp[j];
        ++i;
    }
}

void addRoundKey(uint8_t state[4][4], const uint8_t *roundKey)
{
    for(uint8_t c = 0; c < 4; ++c)
    {
        for(uint8_t r = 0; r < 4; ++r)
            state[r][c] ^= roundKey[c * 4 + r];
    }
}

void subBytes(uint8_t state[4][4])
{
    for(uint8_t r = 0; r < 4; ++r)
        for(uint8_t c = 0; c < 4; ++c)
            state[r][c] = sbox[state[r][c]];
}

void shiftRows(uint8_t state[4][4])
{
    uint8_t tmp;

    tmp = state[1][0];
    state[1][0] = state[1][1];
    state[1][1] = state[1][2];
    state[1][2] = state[1][3];
    state[1][3] = tmp;

    tmp = state[2][0];
    state[2][0] = state[2][2];
    state[2][2] = tmp;
    tmp = state[2][1];
    state[2][1] = state[2][3];
    state[2][3] = tmp;

    tmp = state[3][3];
    state[3][3] = state[3][2];
    state[3][2] = state[3][1];
    state[3][1] = state[3][0];
    state[3][0] = tmp;
}

void mixColumns(uint8_t state[4][4])
{
    for(uint8_t c = 0; c < 4; ++c)
    {
        uint8_t s0 = state[0][c];
        uint8_t s1 = state[1][c];
        uint8_t s2 = state[2][c];
        uint8_t s3 = state[3][c];

        uint8_t t = s0 ^ s1 ^ s2 ^ s3;
        uint8_t u = s0;

        state[0][c] ^= t ^ xtime(static_cast<uint8_t>(s0 ^ s1));
        state[1][c] ^= t ^ xtime(static_cast<uint8_t>(s1 ^ s2));
        state[2][c] ^= t ^ xtime(static_cast<uint8_t>(s2 ^ s3));
        state[3][c] ^= t ^ xtime(static_cast<uint8_t>(s3 ^ u));
    }
}

void aes128EncryptBlock(const uint8_t key[16], const uint8_t input[16],
                        uint8_t output[16])
{
    uint8_t roundKey[176];
    uint8_t state[4][4];

    keyExpansion(key, roundKey);

    for(uint8_t c = 0; c < 4; ++c)
        for(uint8_t r = 0; r < 4; ++r)
            state[r][c] = input[c * 4 + r];

    addRoundKey(state, roundKey);

    for(uint8_t round = 1; round < AES_NR; ++round)
    {
        subBytes(state);
        shiftRows(state);
        mixColumns(state);
        addRoundKey(state, &roundKey[round * 16]);
    }

    subBytes(state);
    shiftRows(state);
    addRoundKey(state, &roundKey[AES_NR * 16]);

    for(uint8_t c = 0; c < 4; ++c)
        for(uint8_t r = 0; r < 4; ++r)
            output[c * 4 + r] = state[r][c];
}

uint8_t hexToNibble(char c)
{
    if((c >= '0') && (c <= '9')) return static_cast<uint8_t>(c - '0');
    if((c >= 'a') && (c <= 'f')) return static_cast<uint8_t>(c - 'a' + 10);
    if((c >= 'A') && (c <= 'F')) return static_cast<uint8_t>(c - 'A' + 10);
    return 0xFF;
}

uint32_t getScramblerSeed(const std::array<uint8_t, 16>& key, uint8_t subtype)
{
    switch(subtype)
    {
        case M17_SCRAMBLING_8BIT:
            return key[0];
        case M17_SCRAMBLING_16BIT:
            return (key[0] << 8) | key[1];
        case M17_SCRAMBLING_24BIT:
        default:
            return (key[0] << 16) | (key[1] << 8) | key[2];
    }
}

uint32_t nextScramblerBit(uint32_t& state, uint8_t subtype)
{
    uint8_t len = 24;
    uint32_t feedback = 0;
    uint32_t mask = 0xFFFFFF;

    switch(subtype)
    {
        case M17_SCRAMBLING_8BIT:
            len = 8;
            mask = 0xFF;
            feedback = ((state >> 7) ^ (state >> 5) ^ (state >> 4) ^ (state >> 3)) & 1;
            break;
        case M17_SCRAMBLING_16BIT:
            len = 16;
            mask = 0xFFFF;
            feedback = ((state >> 15) ^ (state >> 14) ^ (state >> 12) ^ (state >> 3)) & 1;
            break;
        case M17_SCRAMBLING_24BIT:
        default:
            feedback = ((state >> 23) ^ (state >> 22) ^ (state >> 21) ^ (state >> 16)) & 1;
            break;
    }

    state = ((state << 1) | feedback) & mask;
    if(state == 0)
        state = 1;

    (void) len;
    return feedback;
}

}

bool M17Crypto::parseHexKey(const char *hex, std::array<uint8_t, 16>& key,
                            size_t& keyLen)
{
    key.fill(0);
    keyLen = 0;

    if(hex == nullptr)
        return false;

    const size_t len = strnlen(hex, 32);
    if((len == 0) || (len > 32))
        return false;

    if(((len & 1) == 0) && (len <= 32))
    {
        bool isHex = true;
        keyLen = len / 2;
        for(size_t i = 0; i < keyLen; ++i)
        {
            uint8_t hi = hexToNibble(hex[i * 2]);
            uint8_t lo = hexToNibble(hex[i * 2 + 1]);
            if((hi == 0xFF) || (lo == 0xFF))
            {
                isHex = false;
                break;
            }

            key[i] = static_cast<uint8_t>((hi << 4) | lo);
        }

        if(isHex)
            return true;

        key.fill(0);
        keyLen = 0;
    }

    keyLen = (len > 16) ? 16 : len;
    for(size_t i = 0; i < keyLen; ++i)
        key[i] = static_cast<uint8_t>(hex[i]);

    if(len < 16)
        keyLen = 16;

    return true;
}

void M17Crypto::encrypt(const M17LinkSetupFrame& lsf, payload_t& payload,
                        const std::array<uint8_t, 16>& key, size_t keyLen,
                        uint16_t frameNumber)
{
    streamType_t type = lsf.getType();
    switch(type.fields.encType)
    {
        case M17_ENCRYPTION_SCRAMBLER:
            if(keyLen > 0)
                applyScrambler(payload, getScramblerSeed(key, type.fields.encSubType),
                               type.fields.encSubType);
            break;

        case M17_ENCRYPTION_AES:
            if(keyLen >= 16)
                applyAesCtr(payload, lsf.metadata(), key, frameNumber & 0x7FFF);
            break;

        default:
            break;
    }
}

void M17Crypto::fillAesMeta(meta_t& meta, uint32_t timestamp)
{
    for(size_t i = 0; i < 14; ++i)
        meta.raw_data[i] = 0;

    meta.raw_data[0] = (timestamp >> 24) & 0xFF;
    meta.raw_data[1] = (timestamp >> 16) & 0xFF;
    meta.raw_data[2] = (timestamp >> 8) & 0xFF;
    meta.raw_data[3] = timestamp & 0xFF;
    meta.raw_data[4] = 0;
    meta.raw_data[5] = 0;
    meta.raw_data[6] = 0;
    meta.raw_data[7] = 0;
    meta.raw_data[8] = 0;
    meta.raw_data[9] = 0;
    meta.raw_data[10] = 0;
    meta.raw_data[11] = 0;
    meta.raw_data[12] = 0;
    meta.raw_data[13] = 0;
}

void M17Crypto::applyScrambler(payload_t& payload, uint32_t seed,
                               uint8_t subtype)
{
    if(seed == 0)
        seed = 1;

    for(size_t i = 0; i < payload.size(); ++i)
    {
        uint8_t mask = 0;
        for(uint8_t bit = 0; bit < 8; ++bit)
            mask = static_cast<uint8_t>((mask << 1) | nextScramblerBit(seed, subtype));
        payload[i] ^= mask;
    }
}

void M17Crypto::applyAesCtr(payload_t& payload, const meta_t& meta,
                            const std::array<uint8_t, 16>& key,
                            uint16_t frameNumber)
{
    uint8_t counterBlock[16];
    uint8_t streamBlock[16];

    memcpy(counterBlock, meta.raw_data, 14);
    counterBlock[14] = (frameNumber >> 8) & 0xFF;
    counterBlock[15] = frameNumber & 0xFF;

    aes128EncryptBlock(key.data(), counterBlock, streamBlock);
    for(size_t i = 0; i < payload.size(); ++i)
        payload[i] ^= streamBlock[i];
}
