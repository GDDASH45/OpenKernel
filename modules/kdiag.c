/*
 * OpenKernel diagnostic module.
 *
 * This module runs deterministic, side-effect-free tests of low-level
 * algorithms and data structures. It does not probe hardware, modify kernel
 * state, or allocate memory. The exhaustive byte-domain reference tables
 * make regressions in bit and checksum primitives reproducible.
 */
#include <stdint.h>
#include <stddef.h>
#include <kernel/module.h>
#include <write/write.h>

#define KDIAG_RING_CAPACITY 16u
#define KDIAG_BITMAP_BITS 256u
#define KDIAG_MAX_TESTS 4096u

static uint32_t kdiag_checks;
static uint32_t kdiag_failures;

static const uint8_t kdiag_reverse_reference[256] = {
    0x00,
    0x80,
    0x40,
    0xC0,
    0x20,
    0xA0,
    0x60,
    0xE0,
    0x10,
    0x90,
    0x50,
    0xD0,
    0x30,
    0xB0,
    0x70,
    0xF0,
    0x08,
    0x88,
    0x48,
    0xC8,
    0x28,
    0xA8,
    0x68,
    0xE8,
    0x18,
    0x98,
    0x58,
    0xD8,
    0x38,
    0xB8,
    0x78,
    0xF8,
    0x04,
    0x84,
    0x44,
    0xC4,
    0x24,
    0xA4,
    0x64,
    0xE4,
    0x14,
    0x94,
    0x54,
    0xD4,
    0x34,
    0xB4,
    0x74,
    0xF4,
    0x0C,
    0x8C,
    0x4C,
    0xCC,
    0x2C,
    0xAC,
    0x6C,
    0xEC,
    0x1C,
    0x9C,
    0x5C,
    0xDC,
    0x3C,
    0xBC,
    0x7C,
    0xFC,
    0x02,
    0x82,
    0x42,
    0xC2,
    0x22,
    0xA2,
    0x62,
    0xE2,
    0x12,
    0x92,
    0x52,
    0xD2,
    0x32,
    0xB2,
    0x72,
    0xF2,
    0x0A,
    0x8A,
    0x4A,
    0xCA,
    0x2A,
    0xAA,
    0x6A,
    0xEA,
    0x1A,
    0x9A,
    0x5A,
    0xDA,
    0x3A,
    0xBA,
    0x7A,
    0xFA,
    0x06,
    0x86,
    0x46,
    0xC6,
    0x26,
    0xA6,
    0x66,
    0xE6,
    0x16,
    0x96,
    0x56,
    0xD6,
    0x36,
    0xB6,
    0x76,
    0xF6,
    0x0E,
    0x8E,
    0x4E,
    0xCE,
    0x2E,
    0xAE,
    0x6E,
    0xEE,
    0x1E,
    0x9E,
    0x5E,
    0xDE,
    0x3E,
    0xBE,
    0x7E,
    0xFE,
    0x01,
    0x81,
    0x41,
    0xC1,
    0x21,
    0xA1,
    0x61,
    0xE1,
    0x11,
    0x91,
    0x51,
    0xD1,
    0x31,
    0xB1,
    0x71,
    0xF1,
    0x09,
    0x89,
    0x49,
    0xC9,
    0x29,
    0xA9,
    0x69,
    0xE9,
    0x19,
    0x99,
    0x59,
    0xD9,
    0x39,
    0xB9,
    0x79,
    0xF9,
    0x05,
    0x85,
    0x45,
    0xC5,
    0x25,
    0xA5,
    0x65,
    0xE5,
    0x15,
    0x95,
    0x55,
    0xD5,
    0x35,
    0xB5,
    0x75,
    0xF5,
    0x0D,
    0x8D,
    0x4D,
    0xCD,
    0x2D,
    0xAD,
    0x6D,
    0xED,
    0x1D,
    0x9D,
    0x5D,
    0xDD,
    0x3D,
    0xBD,
    0x7D,
    0xFD,
    0x03,
    0x83,
    0x43,
    0xC3,
    0x23,
    0xA3,
    0x63,
    0xE3,
    0x13,
    0x93,
    0x53,
    0xD3,
    0x33,
    0xB3,
    0x73,
    0xF3,
    0x0B,
    0x8B,
    0x4B,
    0xCB,
    0x2B,
    0xAB,
    0x6B,
    0xEB,
    0x1B,
    0x9B,
    0x5B,
    0xDB,
    0x3B,
    0xBB,
    0x7B,
    0xFB,
    0x07,
    0x87,
    0x47,
    0xC7,
    0x27,
    0xA7,
    0x67,
    0xE7,
    0x17,
    0x97,
    0x57,
    0xD7,
    0x37,
    0xB7,
    0x77,
    0xF7,
    0x0F,
    0x8F,
    0x4F,
    0xCF,
    0x2F,
    0xAF,
    0x6F,
    0xEF,
    0x1F,
    0x9F,
    0x5F,
    0xDF,
    0x3F,
    0xBF,
    0x7F,
    0xFF,
};

static const uint8_t kdiag_popcount_reference[256] = {
    0x00,
    0x01,
    0x01,
    0x02,
    0x01,
    0x02,
    0x02,
    0x03,
    0x01,
    0x02,
    0x02,
    0x03,
    0x02,
    0x03,
    0x03,
    0x04,
    0x01,
    0x02,
    0x02,
    0x03,
    0x02,
    0x03,
    0x03,
    0x04,
    0x02,
    0x03,
    0x03,
    0x04,
    0x03,
    0x04,
    0x04,
    0x05,
    0x01,
    0x02,
    0x02,
    0x03,
    0x02,
    0x03,
    0x03,
    0x04,
    0x02,
    0x03,
    0x03,
    0x04,
    0x03,
    0x04,
    0x04,
    0x05,
    0x02,
    0x03,
    0x03,
    0x04,
    0x03,
    0x04,
    0x04,
    0x05,
    0x03,
    0x04,
    0x04,
    0x05,
    0x04,
    0x05,
    0x05,
    0x06,
    0x01,
    0x02,
    0x02,
    0x03,
    0x02,
    0x03,
    0x03,
    0x04,
    0x02,
    0x03,
    0x03,
    0x04,
    0x03,
    0x04,
    0x04,
    0x05,
    0x02,
    0x03,
    0x03,
    0x04,
    0x03,
    0x04,
    0x04,
    0x05,
    0x03,
    0x04,
    0x04,
    0x05,
    0x04,
    0x05,
    0x05,
    0x06,
    0x02,
    0x03,
    0x03,
    0x04,
    0x03,
    0x04,
    0x04,
    0x05,
    0x03,
    0x04,
    0x04,
    0x05,
    0x04,
    0x05,
    0x05,
    0x06,
    0x03,
    0x04,
    0x04,
    0x05,
    0x04,
    0x05,
    0x05,
    0x06,
    0x04,
    0x05,
    0x05,
    0x06,
    0x05,
    0x06,
    0x06,
    0x07,
    0x01,
    0x02,
    0x02,
    0x03,
    0x02,
    0x03,
    0x03,
    0x04,
    0x02,
    0x03,
    0x03,
    0x04,
    0x03,
    0x04,
    0x04,
    0x05,
    0x02,
    0x03,
    0x03,
    0x04,
    0x03,
    0x04,
    0x04,
    0x05,
    0x03,
    0x04,
    0x04,
    0x05,
    0x04,
    0x05,
    0x05,
    0x06,
    0x02,
    0x03,
    0x03,
    0x04,
    0x03,
    0x04,
    0x04,
    0x05,
    0x03,
    0x04,
    0x04,
    0x05,
    0x04,
    0x05,
    0x05,
    0x06,
    0x03,
    0x04,
    0x04,
    0x05,
    0x04,
    0x05,
    0x05,
    0x06,
    0x04,
    0x05,
    0x05,
    0x06,
    0x05,
    0x06,
    0x06,
    0x07,
    0x02,
    0x03,
    0x03,
    0x04,
    0x03,
    0x04,
    0x04,
    0x05,
    0x03,
    0x04,
    0x04,
    0x05,
    0x04,
    0x05,
    0x05,
    0x06,
    0x03,
    0x04,
    0x04,
    0x05,
    0x04,
    0x05,
    0x05,
    0x06,
    0x04,
    0x05,
    0x05,
    0x06,
    0x05,
    0x06,
    0x06,
    0x07,
    0x03,
    0x04,
    0x04,
    0x05,
    0x04,
    0x05,
    0x05,
    0x06,
    0x04,
    0x05,
    0x05,
    0x06,
    0x05,
    0x06,
    0x06,
    0x07,
    0x04,
    0x05,
    0x05,
    0x06,
    0x05,
    0x06,
    0x06,
    0x07,
    0x05,
    0x06,
    0x06,
    0x07,
    0x06,
    0x07,
    0x07,
    0x08,
};

static const uint8_t kdiag_parity_reference[256] = {
    0x00,
    0x01,
    0x01,
    0x00,
    0x01,
    0x00,
    0x00,
    0x01,
    0x01,
    0x00,
    0x00,
    0x01,
    0x00,
    0x01,
    0x01,
    0x00,
    0x01,
    0x00,
    0x00,
    0x01,
    0x00,
    0x01,
    0x01,
    0x00,
    0x00,
    0x01,
    0x01,
    0x00,
    0x01,
    0x00,
    0x00,
    0x01,
    0x01,
    0x00,
    0x00,
    0x01,
    0x00,
    0x01,
    0x01,
    0x00,
    0x00,
    0x01,
    0x01,
    0x00,
    0x01,
    0x00,
    0x00,
    0x01,
    0x00,
    0x01,
    0x01,
    0x00,
    0x01,
    0x00,
    0x00,
    0x01,
    0x01,
    0x00,
    0x00,
    0x01,
    0x00,
    0x01,
    0x01,
    0x00,
    0x01,
    0x00,
    0x00,
    0x01,
    0x00,
    0x01,
    0x01,
    0x00,
    0x00,
    0x01,
    0x01,
    0x00,
    0x01,
    0x00,
    0x00,
    0x01,
    0x00,
    0x01,
    0x01,
    0x00,
    0x01,
    0x00,
    0x00,
    0x01,
    0x01,
    0x00,
    0x00,
    0x01,
    0x00,
    0x01,
    0x01,
    0x00,
    0x00,
    0x01,
    0x01,
    0x00,
    0x01,
    0x00,
    0x00,
    0x01,
    0x01,
    0x00,
    0x00,
    0x01,
    0x00,
    0x01,
    0x01,
    0x00,
    0x01,
    0x00,
    0x00,
    0x01,
    0x00,
    0x01,
    0x01,
    0x00,
    0x00,
    0x01,
    0x01,
    0x00,
    0x01,
    0x00,
    0x00,
    0x01,
    0x01,
    0x00,
    0x00,
    0x01,
    0x00,
    0x01,
    0x01,
    0x00,
    0x00,
    0x01,
    0x01,
    0x00,
    0x01,
    0x00,
    0x00,
    0x01,
    0x00,
    0x01,
    0x01,
    0x00,
    0x01,
    0x00,
    0x00,
    0x01,
    0x01,
    0x00,
    0x00,
    0x01,
    0x00,
    0x01,
    0x01,
    0x00,
    0x00,
    0x01,
    0x01,
    0x00,
    0x01,
    0x00,
    0x00,
    0x01,
    0x01,
    0x00,
    0x00,
    0x01,
    0x00,
    0x01,
    0x01,
    0x00,
    0x01,
    0x00,
    0x00,
    0x01,
    0x00,
    0x01,
    0x01,
    0x00,
    0x00,
    0x01,
    0x01,
    0x00,
    0x01,
    0x00,
    0x00,
    0x01,
    0x00,
    0x01,
    0x01,
    0x00,
    0x01,
    0x00,
    0x00,
    0x01,
    0x01,
    0x00,
    0x00,
    0x01,
    0x00,
    0x01,
    0x01,
    0x00,
    0x01,
    0x00,
    0x00,
    0x01,
    0x00,
    0x01,
    0x01,
    0x00,
    0x00,
    0x01,
    0x01,
    0x00,
    0x01,
    0x00,
    0x00,
    0x01,
    0x01,
    0x00,
    0x00,
    0x01,
    0x00,
    0x01,
    0x01,
    0x00,
    0x00,
    0x01,
    0x01,
    0x00,
    0x01,
    0x00,
    0x00,
    0x01,
    0x00,
    0x01,
    0x01,
    0x00,
    0x01,
    0x00,
    0x00,
    0x01,
    0x01,
    0x00,
    0x00,
    0x01,
    0x00,
    0x01,
    0x01,
    0x00,
};

static const uint8_t kdiag_crc8_reference[256] = {
    0x00,
    0x07,
    0x0E,
    0x09,
    0x1C,
    0x1B,
    0x12,
    0x15,
    0x38,
    0x3F,
    0x36,
    0x31,
    0x24,
    0x23,
    0x2A,
    0x2D,
    0x70,
    0x77,
    0x7E,
    0x79,
    0x6C,
    0x6B,
    0x62,
    0x65,
    0x48,
    0x4F,
    0x46,
    0x41,
    0x54,
    0x53,
    0x5A,
    0x5D,
    0xE0,
    0xE7,
    0xEE,
    0xE9,
    0xFC,
    0xFB,
    0xF2,
    0xF5,
    0xD8,
    0xDF,
    0xD6,
    0xD1,
    0xC4,
    0xC3,
    0xCA,
    0xCD,
    0x90,
    0x97,
    0x9E,
    0x99,
    0x8C,
    0x8B,
    0x82,
    0x85,
    0xA8,
    0xAF,
    0xA6,
    0xA1,
    0xB4,
    0xB3,
    0xBA,
    0xBD,
    0xC7,
    0xC0,
    0xC9,
    0xCE,
    0xDB,
    0xDC,
    0xD5,
    0xD2,
    0xFF,
    0xF8,
    0xF1,
    0xF6,
    0xE3,
    0xE4,
    0xED,
    0xEA,
    0xB7,
    0xB0,
    0xB9,
    0xBE,
    0xAB,
    0xAC,
    0xA5,
    0xA2,
    0x8F,
    0x88,
    0x81,
    0x86,
    0x93,
    0x94,
    0x9D,
    0x9A,
    0x27,
    0x20,
    0x29,
    0x2E,
    0x3B,
    0x3C,
    0x35,
    0x32,
    0x1F,
    0x18,
    0x11,
    0x16,
    0x03,
    0x04,
    0x0D,
    0x0A,
    0x57,
    0x50,
    0x59,
    0x5E,
    0x4B,
    0x4C,
    0x45,
    0x42,
    0x6F,
    0x68,
    0x61,
    0x66,
    0x73,
    0x74,
    0x7D,
    0x7A,
    0x89,
    0x8E,
    0x87,
    0x80,
    0x95,
    0x92,
    0x9B,
    0x9C,
    0xB1,
    0xB6,
    0xBF,
    0xB8,
    0xAD,
    0xAA,
    0xA3,
    0xA4,
    0xF9,
    0xFE,
    0xF7,
    0xF0,
    0xE5,
    0xE2,
    0xEB,
    0xEC,
    0xC1,
    0xC6,
    0xCF,
    0xC8,
    0xDD,
    0xDA,
    0xD3,
    0xD4,
    0x69,
    0x6E,
    0x67,
    0x60,
    0x75,
    0x72,
    0x7B,
    0x7C,
    0x51,
    0x56,
    0x5F,
    0x58,
    0x4D,
    0x4A,
    0x43,
    0x44,
    0x19,
    0x1E,
    0x17,
    0x10,
    0x05,
    0x02,
    0x0B,
    0x0C,
    0x21,
    0x26,
    0x2F,
    0x28,
    0x3D,
    0x3A,
    0x33,
    0x34,
    0x4E,
    0x49,
    0x40,
    0x47,
    0x52,
    0x55,
    0x5C,
    0x5B,
    0x76,
    0x71,
    0x78,
    0x7F,
    0x6A,
    0x6D,
    0x64,
    0x63,
    0x3E,
    0x39,
    0x30,
    0x37,
    0x22,
    0x25,
    0x2C,
    0x2B,
    0x06,
    0x01,
    0x08,
    0x0F,
    0x1A,
    0x1D,
    0x14,
    0x13,
    0xAE,
    0xA9,
    0xA0,
    0xA7,
    0xB2,
    0xB5,
    0xBC,
    0xBB,
    0x96,
    0x91,
    0x98,
    0x9F,
    0x8A,
    0x8D,
    0x84,
    0x83,
    0xDE,
    0xD9,
    0xD0,
    0xD7,
    0xC2,
    0xC5,
    0xCC,
    0xCB,
    0xE6,
    0xE1,
    0xE8,
    0xEF,
    0xFA,
    0xFD,
    0xF4,
    0xF3,
};


static void kdiag_expect(uint32_t actual, uint32_t expected)
{
    ++kdiag_checks;
    if (actual != expected)
        ++kdiag_failures;
}

static void kdiag_expect_true(int condition)
{
    kdiag_expect(condition != 0, 1);
}

static uint8_t kdiag_reverse8(uint8_t value)
{
    uint8_t result = 0;
    for (uint32_t bit = 0; bit < 8; ++bit) {
        result = (uint8_t)((result << 1) | (value & 1u));
        value >>= 1;
    }
    return result;
}

static uint8_t kdiag_popcount8(uint8_t value)
{
    uint8_t count = 0;
    while (value != 0) {
        count = (uint8_t)(count + (value & 1u));
        value >>= 1;
    }
    return count;
}

static uint8_t kdiag_parity8(uint8_t value)
{
    value ^= value >> 4;
    value ^= value >> 2;
    value ^= value >> 1;
    return value & 1u;
}

static uint8_t kdiag_crc8_byte(uint8_t crc, uint8_t value)
{
    crc ^= value;
    for (uint32_t bit = 0; bit < 8; ++bit)
        crc = (crc & 0x80u) ?
              (uint8_t)((crc << 1) ^ 0x07u) :
              (uint8_t)(crc << 1);
    return crc;
}

static uint32_t kdiag_crc32(const uint8_t *data, uint32_t length)
{
    uint32_t crc = 0xFFFFFFFFu;
    for (uint32_t i = 0; i < length; ++i) {
        crc ^= data[i];
        for (uint32_t bit = 0; bit < 8; ++bit)
            crc = (crc >> 1) ^ ((crc & 1u) ? 0xEDB88320u : 0u);
    }
    return crc ^ 0xFFFFFFFFu;
}

static uint32_t kdiag_fnv1a(const uint8_t *data, uint32_t length)
{
    uint32_t hash = 2166136261u;
    for (uint32_t i = 0; i < length; ++i) {
        hash ^= data[i];
        hash *= 16777619u;
    }
    return hash;
}

static int kdiag_equal(const char *left, const char *right)
{
    if (left == 0 || right == 0)
        return left == right;
    while (*left != 0 && *left == *right) {
        ++left;
        ++right;
    }
    return *left == *right;
}

static uint32_t kdiag_strlen(const char *text)
{
    uint32_t length = 0;
    if (text == 0)
        return 0;
    while (text[length] != 0)
        ++length;
    return length;
}

static int kdiag_starts_with(const char *text, const char *prefix)
{
    if (text == 0 || prefix == 0)
        return 0;
    while (*prefix != 0) {
        if (*text++ != *prefix++)
            return 0;
    }
    return 1;
}

static int kdiag_parse_u32(const char *text, uint32_t *value)
{
    uint32_t base = 10;
    uint32_t result = 0;
    uint32_t digits = 0;
    if (text == 0 || value == 0 || *text == 0)
        return -1;
    if (text[0] == '0' && (text[1] == 'x' || text[1] == 'X')) {
        base = 16;
        text += 2;
    }
    if (*text == 0)
        return -1;
    while (*text != 0) {
        uint32_t digit;
        if (*text >= '0' && *text <= '9')
            digit = (uint32_t)(*text - '0');
        else if (base == 16 && *text >= 'a' && *text <= 'f')
            digit = (uint32_t)(*text - 'a') + 10u;
        else if (base == 16 && *text >= 'A' && *text <= 'F')
            digit = (uint32_t)(*text - 'A') + 10u;
        else
            return -1;
        if (digit >= base || result > (0xFFFFFFFFu - digit) / base)
            return -1;
        result = result * base + digit;
        ++digits;
        ++text;
    }
    if (digits == 0)
        return -1;
    *value = result;
    return 0;
}

static int kdiag_add_u32(uint32_t left, uint32_t right, uint32_t *result)
{
    if (result == 0 || left > 0xFFFFFFFFu - right)
        return -1;
    *result = left + right;
    return 0;
}

static int kdiag_mul_u32(uint32_t left, uint32_t right, uint32_t *result)
{
    if (result == 0 || (right != 0 && left > 0xFFFFFFFFu / right))
        return -1;
    *result = left * right;
    return 0;
}

static uint32_t kdiag_load_le32(const uint8_t *bytes)
{
    return (uint32_t)bytes[0] |
           ((uint32_t)bytes[1] << 8) |
           ((uint32_t)bytes[2] << 16) |
           ((uint32_t)bytes[3] << 24);
}

static void kdiag_store_le32(uint8_t *bytes, uint32_t value)
{
    bytes[0] = (uint8_t)value;
    bytes[1] = (uint8_t)(value >> 8);
    bytes[2] = (uint8_t)(value >> 16);
    bytes[3] = (uint8_t)(value >> 24);
}

static uint32_t kdiag_min_u32(uint32_t left, uint32_t right)
{
    return left < right ? left : right;
}

static uint32_t kdiag_max_u32(uint32_t left, uint32_t right)
{
    return left > right ? left : right;
}

static uint32_t kdiag_clamp_u32(uint32_t value,
                                uint32_t minimum,
                                uint32_t maximum)
{
    if (minimum > maximum)
        return minimum;
    if (value < minimum)
        return minimum;
    if (value > maximum)
        return maximum;
    return value;
}

static uint32_t kdiag_gcd(uint32_t left, uint32_t right)
{
    while (right != 0) {
        uint32_t remainder = left % right;
        left = right;
        right = remainder;
    }
    return left;
}

static int kdiag_is_power_of_two(uint32_t value)
{
    return value != 0 && (value & (value - 1u)) == 0;
}

static uint32_t kdiag_round_up_power_of_two(uint32_t value)
{
    if (value <= 1)
        return 1;
    if (value > 0x80000000u)
        return 0;
    --value;
    value |= value >> 1;
    value |= value >> 2;
    value |= value >> 4;
    value |= value >> 8;
    value |= value >> 16;
    return value + 1u;
}

static int kdiag_ascii_digit(uint8_t value)
{
    return value >= '0' && value <= '9';
}

static int kdiag_ascii_hex(uint8_t value)
{
    return kdiag_ascii_digit(value) ||
           (value >= 'a' && value <= 'f') ||
           (value >= 'A' && value <= 'F');
}

static int kdiag_ascii_printable(uint8_t value)
{
    return value >= 0x20 && value <= 0x7E;
}

static uint8_t kdiag_ascii_upper(uint8_t value)
{
    if (value >= 'a' && value <= 'z')
        return (uint8_t)(value - 'a' + 'A');
    return value;
}

static uint8_t kdiag_ascii_lower(uint8_t value)
{
    if (value >= 'A' && value <= 'Z')
        return (uint8_t)(value - 'A' + 'a');
    return value;
}

static int kdiag_path_valid(const char *path)
{
    uint32_t component_length = 0;
    if (path == 0 || path[0] != '/')
        return 0;
    for (uint32_t i = 0; path[i] != 0; ++i) {
        if (path[i] == '/') {
            if (component_length == 2 &&
                path[i - 1] == '.' && path[i - 2] == '.')
                return 0;
            component_length = 0;
        } else {
            ++component_length;
        }
    }
    if (component_length == 2) {
        uint32_t length = kdiag_strlen(path);
        if (length >= 2 && path[length - 1] == '.' &&
            path[length - 2] == '.')
            return 0;
    }
    return 1;
}

struct kdiag_ring {
    uint8_t bytes[KDIAG_RING_CAPACITY];
    uint32_t head;
    uint32_t tail;
    uint32_t count;
};

static void kdiag_ring_init(struct kdiag_ring *ring)
{
    for (uint32_t i = 0; i < KDIAG_RING_CAPACITY; ++i)
        ring->bytes[i] = 0;
    ring->head = 0;
    ring->tail = 0;
    ring->count = 0;
}

static int kdiag_ring_push(struct kdiag_ring *ring, uint8_t value)
{
    if (ring == 0 || ring->count == KDIAG_RING_CAPACITY)
        return -1;
    ring->bytes[ring->head] = value;
    ring->head = (ring->head + 1u) % KDIAG_RING_CAPACITY;
    ++ring->count;
    return 0;
}

static int kdiag_ring_pop(struct kdiag_ring *ring, uint8_t *value)
{
    if (ring == 0 || value == 0 || ring->count == 0)
        return -1;
    *value = ring->bytes[ring->tail];
    ring->tail = (ring->tail + 1u) % KDIAG_RING_CAPACITY;
    --ring->count;
    return 0;
}

struct kdiag_queue {
    uint32_t values[KDIAG_RING_CAPACITY];
    uint32_t read_index;
    uint32_t write_index;
    uint32_t count;
};

static void kdiag_queue_init(struct kdiag_queue *queue)
{
    queue->read_index = 0;
    queue->write_index = 0;
    queue->count = 0;
    for (uint32_t i = 0; i < KDIAG_RING_CAPACITY; ++i)
        queue->values[i] = 0;
}

static int kdiag_queue_push(struct kdiag_queue *queue, uint32_t value)
{
    if (queue == 0 || queue->count == KDIAG_RING_CAPACITY)
        return -1;
    queue->values[queue->write_index] = value;
    queue->write_index = (queue->write_index + 1u) %
                         KDIAG_RING_CAPACITY;
    ++queue->count;
    return 0;
}

static int kdiag_queue_pop(struct kdiag_queue *queue, uint32_t *value)
{
    if (queue == 0 || value == 0 || queue->count == 0)
        return -1;
    *value = queue->values[queue->read_index];
    queue->read_index = (queue->read_index + 1u) %
                        KDIAG_RING_CAPACITY;
    --queue->count;
    return 0;
}

struct kdiag_stack {
    uint32_t values[KDIAG_RING_CAPACITY];
    uint32_t count;
};

static void kdiag_stack_init(struct kdiag_stack *stack)
{
    stack->count = 0;
    for (uint32_t i = 0; i < KDIAG_RING_CAPACITY; ++i)
        stack->values[i] = 0;
}

static int kdiag_stack_push(struct kdiag_stack *stack, uint32_t value)
{
    if (stack == 0 || stack->count == KDIAG_RING_CAPACITY)
        return -1;
    stack->values[stack->count++] = value;
    return 0;
}

static int kdiag_stack_pop(struct kdiag_stack *stack, uint32_t *value)
{
    if (stack == 0 || value == 0 || stack->count == 0)
        return -1;
    *value = stack->values[--stack->count];
    return 0;
}

struct kdiag_bitmap {
    uint32_t words[KDIAG_BITMAP_BITS / 32u];
};

static void kdiag_bitmap_clear(struct kdiag_bitmap *bitmap)
{
    for (uint32_t i = 0; i < KDIAG_BITMAP_BITS / 32u; ++i)
        bitmap->words[i] = 0;
}

static int kdiag_bitmap_set(struct kdiag_bitmap *bitmap, uint32_t bit)
{
    if (bitmap == 0 || bit >= KDIAG_BITMAP_BITS)
        return -1;
    bitmap->words[bit / 32u] |= 1u << (bit % 32u);
    return 0;
}

static int kdiag_bitmap_test(const struct kdiag_bitmap *bitmap,
                             uint32_t bit)
{
    if (bitmap == 0 || bit >= KDIAG_BITMAP_BITS)
        return -1;
    return (bitmap->words[bit / 32u] >> (bit % 32u)) & 1u;
}

static int kdiag_bitmap_clear_bit(struct kdiag_bitmap *bitmap,
                                  uint32_t bit)
{
    if (bitmap == 0 || bit >= KDIAG_BITMAP_BITS)
        return -1;
    bitmap->words[bit / 32u] &= ~(1u << (bit % 32u));
    return 0;
}

static uint32_t kdiag_bitmap_count(const struct kdiag_bitmap *bitmap)
{
    uint32_t count = 0;
    for (uint32_t i = 0; i < KDIAG_BITMAP_BITS / 32u; ++i) {
        uint32_t value = bitmap->words[i];
        while (value != 0) {
            value &= value - 1u;
            ++count;
        }
    }
    return count;
}

static void kdiag_test_byte_tables(void)
{
    for (uint32_t value = 0; value < 256; ++value) {
        uint8_t byte = (uint8_t)value;
        kdiag_expect(kdiag_reverse8(byte), kdiag_reverse_reference[value]);
        kdiag_expect(kdiag_popcount8(byte), kdiag_popcount_reference[value]);
        kdiag_expect(kdiag_parity8(byte), kdiag_parity_reference[value]);
        kdiag_expect(kdiag_crc8_byte(0, byte), kdiag_crc8_reference[value]);
    }
}

static void kdiag_test_ascii(void)
{
    for (uint32_t value = 0; value < 256; ++value) {
        uint8_t c = (uint8_t)value;
        kdiag_expect(kdiag_ascii_digit(c),
                     value >= '0' && value <= '9');
        kdiag_expect(kdiag_ascii_hex(c),
                     (value >= '0' && value <= '9') ||
                     (value >= 'a' && value <= 'f') ||
                     (value >= 'A' && value <= 'F'));
        kdiag_expect(kdiag_ascii_printable(c),
                     value >= 0x20 && value <= 0x7E);
        if (value >= 'a' && value <= 'z')
            kdiag_expect(kdiag_ascii_upper(c), value - 32u);
        else
            kdiag_expect(kdiag_ascii_upper(c), value);
        if (value >= 'A' && value <= 'Z')
            kdiag_expect(kdiag_ascii_lower(c), value + 32u);
        else
            kdiag_expect(kdiag_ascii_lower(c), value);
    }
}

static void kdiag_test_checksums(void)
{
    static const uint8_t digits[] = {
        '1', '2', '3', '4', '5', '6', '7', '8', '9'
    };
    static const uint8_t empty[] = { 0 };
    static const uint8_t hello[] = {
        'h', 'e', 'l', 'l', 'o'
    };
    kdiag_expect(kdiag_crc32(digits, 9), 0xCBF43926u);
    kdiag_expect(kdiag_crc32(empty, 0), 0x00000000u);
    kdiag_expect(kdiag_fnv1a(hello, 5), 0x4F9F2CABu);
    kdiag_expect(kdiag_fnv1a(empty, 0), 2166136261u);
    kdiag_expect(kdiag_crc8_byte(0, 0), 0);
    kdiag_expect(kdiag_crc8_byte(0, 1), 0x07);
}

static void kdiag_test_integer_helpers(void)
{
    uint32_t result = 0;
    kdiag_expect(kdiag_add_u32(1, 2, &result), 0);
    kdiag_expect(result, 3);
    kdiag_expect(kdiag_add_u32(0xFFFFFFFFu, 1, &result), 0xFFFFFFFFu);
    kdiag_expect(kdiag_add_u32(0xFFFFFFFFu, 1, 0), 0xFFFFFFFFu);
    kdiag_expect(kdiag_add_u32(0xFFFFFFFFu, 1, &result), 0xFFFFFFFFu);
    kdiag_expect(kdiag_mul_u32(12, 13, &result), 0);
    kdiag_expect(result, 156);
    kdiag_expect(kdiag_mul_u32(0xFFFFFFFFu, 2, &result), 0xFFFFFFFFu);
    kdiag_expect(kdiag_mul_u32(0, 0xFFFFFFFFu, &result), 0);
    kdiag_expect(result, 0);
    kdiag_expect(kdiag_min_u32(4, 9), 4);
    kdiag_expect(kdiag_max_u32(4, 9), 9);
    kdiag_expect(kdiag_clamp_u32(5, 1, 8), 5);
    kdiag_expect(kdiag_clamp_u32(0, 1, 8), 1);
    kdiag_expect(kdiag_clamp_u32(9, 1, 8), 8);
    kdiag_expect(kdiag_clamp_u32(5, 9, 1), 9);
    kdiag_expect(kdiag_gcd(54, 24), 6);
    kdiag_expect(kdiag_gcd(17, 13), 1);
    kdiag_expect(kdiag_gcd(0, 9), 9);
    kdiag_expect(kdiag_is_power_of_two(1), 1);
    kdiag_expect(kdiag_is_power_of_two(1024), 1);
    kdiag_expect(kdiag_is_power_of_two(0), 0);
    kdiag_expect(kdiag_is_power_of_two(1023), 0);
    kdiag_expect(kdiag_round_up_power_of_two(0), 1);
    kdiag_expect(kdiag_round_up_power_of_two(1), 1);
    kdiag_expect(kdiag_round_up_power_of_two(3), 4);
    kdiag_expect(kdiag_round_up_power_of_two(1025), 2048);
    kdiag_expect(kdiag_round_up_power_of_two(0x80000001u), 0);
}

static void kdiag_test_integer_parser(void)
{
    uint32_t value = 0;
    kdiag_expect(kdiag_parse_u32("0", &value), 0);
    kdiag_expect(value, 0);
    kdiag_expect(kdiag_parse_u32("42", &value), 0);
    kdiag_expect(value, 42);
    kdiag_expect(kdiag_parse_u32("4294967295", &value), 0);
    kdiag_expect(value, 0xFFFFFFFFu);
    kdiag_expect(kdiag_parse_u32("4294967296", &value), 0xFFFFFFFFu);
    kdiag_expect(kdiag_parse_u32("0x0", &value), 0);
    kdiag_expect(value, 0);
    kdiag_expect(kdiag_parse_u32("0x2A", &value), 0);
    kdiag_expect(value, 42);
    kdiag_expect(kdiag_parse_u32("0xffffffff", &value), 0);
    kdiag_expect(value, 0xFFFFFFFFu);
    kdiag_expect(kdiag_parse_u32("", &value), 0xFFFFFFFFu);
    kdiag_expect(kdiag_parse_u32("0x", &value), 0xFFFFFFFFu);
    kdiag_expect(kdiag_parse_u32("-1", &value), 0xFFFFFFFFu);
    kdiag_expect(kdiag_parse_u32("12x", &value), 0xFFFFFFFFu);
    kdiag_expect(kdiag_parse_u32("0x100000000", &value), 0xFFFFFFFFu);
    kdiag_expect(kdiag_parse_u32(" 1", &value), 0xFFFFFFFFu);
    kdiag_expect(kdiag_parse_u32("1 ", &value), 0xFFFFFFFFu);
    kdiag_expect(kdiag_parse_u32("0XfF", &value), 0);
    kdiag_expect(value, 255);
}

static void kdiag_test_endian_helpers(void)
{
    uint8_t bytes[4];
    kdiag_store_le32(bytes, 0x12345678u);
    kdiag_expect(bytes[0], 0x78);
    kdiag_expect(bytes[1], 0x56);
    kdiag_expect(bytes[2], 0x34);
    kdiag_expect(bytes[3], 0x12);
    kdiag_expect(kdiag_load_le32(bytes), 0x12345678u);
    kdiag_store_le32(bytes, 0);
    kdiag_expect(kdiag_load_le32(bytes), 0);
    kdiag_store_le32(bytes, 0xFFFFFFFFu);
    kdiag_expect(kdiag_load_le32(bytes), 0xFFFFFFFFu);
}

static void kdiag_test_strings(void)
{
    kdiag_expect(kdiag_equal("", ""), 1);
    kdiag_expect(kdiag_equal("open", "open"), 1);
    kdiag_expect(kdiag_equal("open", "Open"), 0);
    kdiag_expect(kdiag_equal("open", "kernel"), 0);
    kdiag_expect(kdiag_equal(0, 0), 1);
    kdiag_expect(kdiag_equal(0, ""), 0);
    kdiag_expect(kdiag_strlen(""), 0);
    kdiag_expect(kdiag_strlen("OpenKernel"), 10);
    kdiag_expect(kdiag_strlen(0), 0);
    kdiag_expect(kdiag_starts_with("OpenKernel", "Open"), 1);
    kdiag_expect(kdiag_starts_with("OpenKernel", "Kernel"), 0);
    kdiag_expect(kdiag_starts_with("", ""), 1);
    kdiag_expect(kdiag_starts_with(0, "x"), 0);
    kdiag_expect(kdiag_starts_with("x", 0), 0);
}

static void kdiag_test_paths(void)
{
    kdiag_expect(kdiag_path_valid("/"), 1);
    kdiag_expect(kdiag_path_valid("/dev/null"), 1);
    kdiag_expect(kdiag_path_valid("/usr/lib/modules/test.ko"), 1);
    kdiag_expect(kdiag_path_valid("relative/path"), 0);
    kdiag_expect(kdiag_path_valid(0), 0);
    kdiag_expect(kdiag_path_valid("/../etc/passwd"), 0);
    kdiag_expect(kdiag_path_valid("/usr/../etc"), 0);
    kdiag_expect(kdiag_path_valid("/usr/lib/.."), 0);
    kdiag_expect(kdiag_path_valid("/usr/."), 1);
    kdiag_expect(kdiag_path_valid("/.../file"), 1);
    kdiag_expect(kdiag_path_valid("/a/b/c"), 1);
}

static void kdiag_test_ring(void)
{
    struct kdiag_ring ring;
    uint8_t value = 0;
    kdiag_ring_init(&ring);
    kdiag_expect(ring.count, 0);
    kdiag_expect(kdiag_ring_pop(&ring, &value), 0xFFFFFFFFu);
    for (uint32_t i = 0; i < KDIAG_RING_CAPACITY; ++i)
        kdiag_expect(kdiag_ring_push(&ring, (uint8_t)i), 0);
    kdiag_expect(ring.count, KDIAG_RING_CAPACITY);
    kdiag_expect(kdiag_ring_push(&ring, 0xFF), 0xFFFFFFFFu);
    for (uint32_t i = 0; i < KDIAG_RING_CAPACITY / 2; ++i) {
        kdiag_expect(kdiag_ring_pop(&ring, &value), 0);
        kdiag_expect(value, i);
    }
    for (uint32_t i = 0; i < KDIAG_RING_CAPACITY / 2; ++i)
        kdiag_expect(kdiag_ring_push(&ring, (uint8_t)(i + 32)), 0);
    for (uint32_t i = KDIAG_RING_CAPACITY / 2; i < KDIAG_RING_CAPACITY; ++i) {
        kdiag_expect(kdiag_ring_pop(&ring, &value), 0);
        kdiag_expect(value, i);
    }
    for (uint32_t i = 0; i < KDIAG_RING_CAPACITY / 2; ++i) {
        kdiag_expect(kdiag_ring_pop(&ring, &value), 0);
        kdiag_expect(value, i + 32);
    }
    kdiag_expect(ring.count, 0);
}

static void kdiag_test_queue(void)
{
    struct kdiag_queue queue;
    uint32_t value = 0;
    kdiag_queue_init(&queue);
    kdiag_expect(kdiag_queue_pop(&queue, &value), 0xFFFFFFFFu);
    for (uint32_t i = 0; i < KDIAG_RING_CAPACITY; ++i)
        kdiag_expect(kdiag_queue_push(&queue, i * 3u), 0);
    kdiag_expect(kdiag_queue_push(&queue, 999), 0xFFFFFFFFu);
    for (uint32_t i = 0; i < KDIAG_RING_CAPACITY; ++i) {
        kdiag_expect(kdiag_queue_pop(&queue, &value), 0);
        kdiag_expect(value, i * 3u);
    }
    kdiag_expect(queue.count, 0);
    for (uint32_t i = 0; i < 40; ++i) {
        kdiag_expect(kdiag_queue_push(&queue, i), 0);
        kdiag_expect(kdiag_queue_pop(&queue, &value), 0);
        kdiag_expect(value, i);
    }
}

static void kdiag_test_stack(void)
{
    struct kdiag_stack stack;
    uint32_t value = 0;
    kdiag_stack_init(&stack);
    kdiag_expect(kdiag_stack_pop(&stack, &value), 0xFFFFFFFFu);
    for (uint32_t i = 0; i < KDIAG_RING_CAPACITY; ++i)
        kdiag_expect(kdiag_stack_push(&stack, i + 100u), 0);
    kdiag_expect(kdiag_stack_push(&stack, 0), 0xFFFFFFFFu);
    for (uint32_t i = KDIAG_RING_CAPACITY; i > 0; --i) {
        kdiag_expect(kdiag_stack_pop(&stack, &value), 0);
        kdiag_expect(value, i + 99u);
    }
    kdiag_expect(stack.count, 0);
}

static void kdiag_test_bitmap(void)
{
    struct kdiag_bitmap bitmap;
    kdiag_bitmap_clear(&bitmap);
    kdiag_expect(kdiag_bitmap_count(&bitmap), 0);
    kdiag_expect(kdiag_bitmap_test(&bitmap, 0), 0);
    for (uint32_t i = 0; i < KDIAG_BITMAP_BITS; ++i)
        kdiag_expect(kdiag_bitmap_set(&bitmap, i), 0);
    kdiag_expect(kdiag_bitmap_count(&bitmap), KDIAG_BITMAP_BITS);
    kdiag_expect(kdiag_bitmap_set(&bitmap, KDIAG_BITMAP_BITS), 0xFFFFFFFFu);
    for (uint32_t i = 0; i < KDIAG_BITMAP_BITS; ++i)
        kdiag_expect(kdiag_bitmap_test(&bitmap, i), 1);
    for (uint32_t i = 0; i < KDIAG_BITMAP_BITS; i += 2)
        kdiag_expect(kdiag_bitmap_clear_bit(&bitmap, i), 0);
    kdiag_expect(kdiag_bitmap_count(&bitmap), KDIAG_BITMAP_BITS / 2);
    for (uint32_t i = 0; i < KDIAG_BITMAP_BITS; ++i)
        kdiag_expect(kdiag_bitmap_test(&bitmap, i), (i & 1u) != 0);
    kdiag_expect(kdiag_bitmap_clear_bit(&bitmap, KDIAG_BITMAP_BITS),
                 0xFFFFFFFFu);
}

static void kdiag_test_alignment(void)
{
    static const uint32_t inputs[] = {
        0, 1, 2, 3, 4, 5, 7, 8, 9, 15, 16, 17,
        31, 32, 33, 63, 64, 65, 127, 128, 129,
        255, 256, 257, 511, 512, 513, 1023, 1024,
        1025, 4095, 4096, 4097
    };
    static const uint32_t outputs[] = {
        1, 1, 2, 4, 4, 8, 8, 8, 16, 16, 16, 32,
        32, 32, 64, 64, 64, 128, 128, 128, 256,
        256, 256, 512, 512, 512, 1024, 1024, 1024,
        2048, 4096, 4096, 8192
    };
    for (uint32_t i = 0; i < sizeof(inputs) / sizeof(inputs[0]); ++i)
        kdiag_expect(kdiag_round_up_power_of_two(inputs[i]), outputs[i]);
}

static void kdiag_test_byte_involution(void)
{
    for (uint32_t value = 0; value < 256; ++value) {
        uint8_t byte = (uint8_t)value;
        kdiag_expect(kdiag_reverse8(kdiag_reverse8(byte)), byte);
        kdiag_expect(kdiag_popcount8(byte) +
                     kdiag_popcount8((uint8_t)~byte), 8);
        kdiag_expect(kdiag_parity8(byte),
                     kdiag_popcount8(byte) & 1u);
    }
}

static void kdiag_test_crc_incremental(void)
{
    static const uint8_t sample[] = {
        'O', 'p', 'e', 'n', 'K', 'e', 'r', 'n', 'e', 'l'
    };
    uint8_t whole = 0;
    uint8_t split = 0;
    for (uint32_t i = 0; i < sizeof(sample); ++i)
        whole = kdiag_crc8_byte(whole, sample[i]);
    for (uint32_t i = 0; i < 4; ++i)
        split = kdiag_crc8_byte(split, sample[i]);
    for (uint32_t i = 4; i < sizeof(sample); ++i)
        split = kdiag_crc8_byte(split, sample[i]);
    kdiag_expect(whole, split);
}

static void kdiag_test_gcd_properties(void)
{
    for (uint32_t a = 1; a <= 32; ++a) {
        for (uint32_t b = 1; b <= 32; ++b) {
            uint32_t gcd = kdiag_gcd(a, b);
            kdiag_expect(gcd != 0, 1);
            kdiag_expect(a % gcd, 0);
            kdiag_expect(b % gcd, 0);
            kdiag_expect(kdiag_gcd(a, b), kdiag_gcd(b, a));
        }
    }
}

static void kdiag_test_ring_stress(void)
{
    struct kdiag_ring ring;
    uint8_t value;
    kdiag_ring_init(&ring);
    for (uint32_t round = 0; round < 64; ++round) {
        for (uint32_t i = 0; i < KDIAG_RING_CAPACITY; ++i)
            kdiag_expect(kdiag_ring_push(&ring,
                (uint8_t)(round + i)), 0);
        kdiag_expect(kdiag_ring_push(&ring, 0), 0xFFFFFFFFu);
        for (uint32_t i = 0; i < KDIAG_RING_CAPACITY; ++i) {
            kdiag_expect(kdiag_ring_pop(&ring, &value), 0);
            kdiag_expect(value, (uint8_t)(round + i));
        }
        kdiag_expect(ring.count, 0);
    }
}

static void kdiag_test_bitmap_patterns(void)
{
    struct kdiag_bitmap bitmap;
    for (uint32_t pattern = 0; pattern < 8; ++pattern) {
        kdiag_bitmap_clear(&bitmap);
        for (uint32_t bit = 0; bit < KDIAG_BITMAP_BITS; ++bit) {
            int set = 0;
            if (pattern == 0) set = 1;
            if (pattern == 1) set = (bit & 1u) == 0;
            if (pattern == 2) set = (bit & 1u) != 0;
            if (pattern == 3) set = (bit % 3u) == 0;
            if (pattern == 4) set = (bit % 5u) == 0;
            if (pattern == 5) set = (bit % 7u) == 0;
            if (pattern == 6) set = bit == 0 || bit == 255;
            if (pattern == 7) set = 0;
            if (set)
                kdiag_expect(kdiag_bitmap_set(&bitmap, bit), 0);
        }
        uint32_t expected = 0;
        for (uint32_t bit = 0; bit < KDIAG_BITMAP_BITS; ++bit) {
            int set = 0;
            if (pattern == 0) set = 1;
            if (pattern == 1) set = (bit & 1u) == 0;
            if (pattern == 2) set = (bit & 1u) != 0;
            if (pattern == 3) set = (bit % 3u) == 0;
            if (pattern == 4) set = (bit % 5u) == 0;
            if (pattern == 5) set = (bit % 7u) == 0;
            if (pattern == 6) set = bit == 0 || bit == 255;
            if (set) ++expected;
            kdiag_expect(kdiag_bitmap_test(&bitmap, bit), set);
        }
        kdiag_expect(kdiag_bitmap_count(&bitmap), expected);
    }
}

static void kdiag_run_suite(void)
{
    kdiag_checks = 0;
    kdiag_failures = 0;
    kdiag_test_byte_tables();
    kdiag_test_ascii();
    kdiag_test_checksums();
    kdiag_test_integer_helpers();
    kdiag_test_integer_parser();
    kdiag_test_endian_helpers();
    kdiag_test_strings();
    kdiag_test_paths();
    kdiag_test_ring();
    kdiag_test_queue();
    kdiag_test_stack();
    kdiag_test_bitmap();
    kdiag_test_alignment();
    kdiag_test_byte_involution();
    kdiag_test_crc_incremental();
    kdiag_test_gcd_properties();
    kdiag_test_ring_stress();
    kdiag_test_bitmap_patterns();
}

static int __init kdiag_init(void)
{
    k_print("[KDIAG] Starting OpenKernel diagnostic suite.\n");
    kdiag_run_suite();
    k_print("[KDIAG] Checks completed: ");
    /* Decimal output without depending on non-exported formatting helpers. */
    {
        char digits[10];
        uint32_t count = 0;
        uint32_t value = kdiag_checks;
        do {
            digits[count++] = (char)('0' + value % 10u);
            value /= 10u;
        } while (value != 0 && count < sizeof(digits));
        while (count != 0)
            k_print_char(digits[--count]);
    }
    k_print("\n[KDIAG] Failures: ");
    {
        char digits[10];
        uint32_t count = 0;
        uint32_t value = kdiag_failures;
        do {
            digits[count++] = (char)('0' + value % 10u);
            value /= 10u;
        } while (value != 0 && count < sizeof(digits));
        while (count != 0)
            k_print_char(digits[--count]);
    }
    k_print("\n");
    if (kdiag_failures != 0) {
        k_print("[KDIAG] FAILED. Module will not remain loaded.\n");
        return -1;
    }
    k_print("[KDIAG] All diagnostic checks passed.\n");
    return 0;
}

static void __exit kdiag_exit(void)
{
    k_print("[KDIAG] Diagnostic module unloaded.\n");
}

KERNEL_MODULE(kdiag, kdiag_init, kdiag_exit);
