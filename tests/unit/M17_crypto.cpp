/*
 * SPDX-FileCopyrightText: Copyright 2020-2026 OpenRTX Contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <catch2/catch_test_macros.hpp>
#include <array>
#include "protocols/M17/M17Crypto.hpp"
#include "protocols/M17/M17LinkSetupFrame.hpp"

using namespace M17;

TEST_CASE("Parse AES key from hex", "[m17][crypto]")
{
    std::array<uint8_t, 16> key;
    size_t keyLen = 0;

    REQUIRE(M17Crypto::parseHexKey("00112233445566778899AABBCCDDEEFF", key, keyLen));
    REQUIRE(keyLen == 16);
    REQUIRE(key[0] == 0x00);
    REQUIRE(key[15] == 0xFF);
}

TEST_CASE("Parse ASCII key fallback", "[m17][crypto]")
{
    std::array<uint8_t, 16> key;
    size_t keyLen = 0;

    REQUIRE(M17Crypto::parseHexKey("swordfish", key, keyLen));
    REQUIRE(keyLen == 16);
    REQUIRE(key[0] == 's');
    REQUIRE(key[8] == 'h');
    REQUIRE(key[9] == 0x00);
}

TEST_CASE("Scrambler payload roundtrip", "[m17][crypto]")
{
    M17LinkSetupFrame lsf;
    streamType_t type = {};
    type.fields.encType = M17_ENCRYPTION_SCRAMBLER;
    type.fields.encSubType = M17_SCRAMBLING_16BIT;
    lsf.setType(type);

    payload_t original = { 0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77,
                           0x88, 0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF };
    payload_t encrypted = original;
    std::array<uint8_t, 16> key = { 0x12, 0x34 };

    M17Crypto::encrypt(lsf, encrypted, key, 2, 7);
    REQUIRE(encrypted != original);
    M17Crypto::decrypt(lsf, encrypted, key, 2, 7);
    REQUIRE(encrypted == original);
}

TEST_CASE("AES payload roundtrip", "[m17][crypto]")
{
    M17LinkSetupFrame lsf;
    streamType_t type = {};
    type.fields.encType = M17_ENCRYPTION_AES;
    lsf.setType(type);
    M17Crypto::fillAesMeta(lsf.metadata(), 0x12345678);
    lsf.metadata().raw_data[4] = 0x9A;
    lsf.metadata().raw_data[5] = 0xBC;
    lsf.metadata().raw_data[6] = 0xDE;
    lsf.metadata().raw_data[7] = 0xF0;
    lsf.metadata().raw_data[8] = 0x11;
    lsf.metadata().raw_data[9] = 0x22;
    lsf.metadata().raw_data[10] = 0x33;
    lsf.metadata().raw_data[11] = 0x44;
    lsf.metadata().raw_data[12] = 0x55;
    lsf.metadata().raw_data[13] = 0x66;

    payload_t original = { 0x10, 0x32, 0x54, 0x76, 0x98, 0xBA, 0xDC, 0xFE,
                           0xEF, 0xCD, 0xAB, 0x89, 0x67, 0x45, 0x23, 0x01 };
    payload_t encrypted = original;
    std::array<uint8_t, 16> key = {
        0x00, 0x11, 0x22, 0x33,
        0x44, 0x55, 0x66, 0x77,
        0x88, 0x99, 0xAA, 0xBB,
        0xCC, 0xDD, 0xEE, 0xFF
    };

    M17Crypto::encrypt(lsf, encrypted, key, 16, 0x1234);
    REQUIRE(encrypted != original);
    M17Crypto::decrypt(lsf, encrypted, key, 16, 0x1234);
    REQUIRE(encrypted == original);
}
