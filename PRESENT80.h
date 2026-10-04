#ifndef PRESENT80_H
#define PRESENT80_H

#include <Arduino.h>

static const uint8_t PRESENT_SBOX[16] = {
  0xC, 0x5, 0x6, 0xB, 0x9, 0x0, 0xA, 0xD, 0x3, 0xE, 0xF, 0x8, 0x4, 0x7, 0x1, 0x2
};

static const uint8_t PRESENT_INV_SBOX[16] = {
  0x5, 0xE, 0xF, 0x8, 0xC, 0x1, 0x2, 0xD, 0xB, 0x4, 0x6, 0x3, 0x0, 0x7, 0x9, 0xA
};

class PRESENT80 {
public:
  explicit PRESENT80(const uint8_t key[10]) { setKey(key); }

  void setKey(const uint8_t key[10]) {
    memcpy(masterKey, key, 10);
    generateRoundKeys();
  }

  uint64_t encryptBlock(uint64_t plaintext) const {
    uint64_t state = plaintext;
    for (uint8_t round = 0; round < 31; round++) {
      state ^= roundKeys[round];
      state = sBoxLayer(state);
      state = pLayer(state);
    }
    return state ^ roundKeys[31];
  }

  uint64_t decryptBlock(uint64_t ciphertext) const {
    uint64_t state = ciphertext ^ roundKeys[31];
    for (int round = 30; round >= 0; round--) {
      state = invPLayer(state);
      state = invSBoxLayer(state);
      state ^= roundKeys[round];
    }
    return state;
  }

private:
  uint8_t masterKey[10];
  uint64_t roundKeys[32];

  static uint64_t sBoxLayer(uint64_t state) {
    uint64_t result = 0;
    for (uint8_t i = 0; i < 16; i++) {
      const uint8_t nibble = (state >> (i * 4)) & 0x0F;
      result |= ((uint64_t)PRESENT_SBOX[nibble]) << (i * 4);
    }
    return result;
  }

  static uint64_t invSBoxLayer(uint64_t state) {
    uint64_t result = 0;
    for (uint8_t i = 0; i < 16; i++) {
      const uint8_t nibble = (state >> (i * 4)) & 0x0F;
      result |= ((uint64_t)PRESENT_INV_SBOX[nibble]) << (i * 4);
    }
    return result;
  }

  static uint64_t pLayer(uint64_t state) {
    uint64_t result = 0;
    for (uint8_t i = 0; i < 63; i++) {
      if ((state >> i) & 1ULL) {
        result |= 1ULL << ((16 * i) % 63);
      }
    }
    if ((state >> 63) & 1ULL) result |= 1ULL << 63;
    return result;
  }

  static uint64_t invPLayer(uint64_t state) {
    uint64_t result = 0;
    for (uint8_t i = 0; i < 63; i++) {
      if ((state >> ((16 * i) % 63)) & 1ULL) {
        result |= 1ULL << i;
      }
    }
    if ((state >> 63) & 1ULL) result |= 1ULL << 63;
    return result;
  }

  void generateRoundKeys() {
    uint16_t high16 = ((uint16_t)masterKey[0] << 8) | masterKey[1];
    uint64_t low64 = 0;
    for (uint8_t i = 2; i < 10; i++) low64 = (low64 << 8) | masterKey[i];

    for (uint8_t round = 1; round <= 32; round++) {
      roundKeys[round - 1] = ((uint64_t)high16 << 48) | (low64 >> 16);
      if (round == 32) break;

      const uint64_t low19 = low64 & 0x7FFFFULL;
      const uint16_t newHigh16 = (uint16_t)(low19 >> 3);
      const uint64_t newLow64 = (low64 >> 19) | ((uint64_t)high16 << 45) | ((low19 & 0x7ULL) << 61);

      high16 = newHigh16;
      low64 = newLow64;

      const uint8_t topNibble = (high16 >> 12) & 0x0F;
      high16 = (uint16_t)((PRESENT_SBOX[topNibble] << 12) | (high16 & 0x0FFF));
      low64 ^= ((uint64_t)(round & 0x1F)) << 15;
    }
  }
};

#endif