/* On-board test for STM32H753. Flash cryp_h753_kat.elf and inspect
 * cryp_test_result in the debugger: 0 is success and -1 means pending.
 * Reference tags were generated independently with PyCryptodome AES.
 */
#include "cryp/uni_hal_cryp.h"
#include <stm32h7xx.h>
#include <string.h>

volatile int cryp_test_result = -1;

static const uint8_t tags[2][2][16] = {
    {
        {0xd6, 0x94, 0xd9, 0x0d, 0x2b, 0x1c, 0x55, 0xc1, 0x86, 0xa2, 0xbe, 0xc8, 0x8f, 0x84, 0x96, 0xac},
        {0xb5, 0x3d, 0x70, 0x0b, 0xec, 0xa8, 0xb7, 0x7d, 0x8d, 0x0c, 0xee, 0x10, 0x04, 0x5a, 0x85, 0xc5},
    },
    {
        {0x2b, 0x07, 0x06, 0xa8, 0x5a, 0xc6, 0x95, 0x71, 0x82, 0x24, 0xef, 0xfa, 0x3b, 0x49, 0x70, 0x04},
        {0xe9, 0x5f, 0x91, 0xe2, 0x26, 0xb6, 0x0b, 0x11, 0x54, 0x1e, 0xdc, 0xa4, 0x87, 0xea, 0x02, 0x0f},
    },
};

static int run_vectors(void)
{
    uint8_t key[32], nonce[13], aad[19];
    for (size_t i = 0; i < sizeof(key); ++i) key[i] = (uint8_t)i;
    for (size_t i = 0; i < sizeof(nonce); ++i) nonce[i] = (uint8_t)i;
    for (size_t i = 0; i < sizeof(aad); ++i) aad[i] = (uint8_t)i;
    alignas(4) uint8_t storage[259];

    for (unsigned mode = 0; mode < 2; ++mode) {
        for (size_t tail = 0; tail <= 1; ++tail) {
            const uni_hal_cryp_message_t m = {
                .mode = mode ? UNI_HAL_CRYP_AES_CCM : UNI_HAL_CRYP_AES_GCM,
                .key = key, .key_len = sizeof(key), .nonce = nonce, .nonce_len = mode ? 13 : 12,
                .aad = aad, .aad_len = sizeof(aad), .input = storage + 1, .output = storage + 1,
                .size = 256 + tail
            };
            storage[0] = storage[m.size + 1] = 0xa5;
            for (size_t i = 0; i < m.size; ++i) m.output[i] = (uint8_t)i;
            uint8_t tag[16];
            uni_hal_cryp_status_e rc = uni_hal_cryp_encrypt(&m, tag, sizeof(tag));
            if (tail && (DBGMCU->IDCODE >> DBGMCU_IDCODE_REV_ID_Pos) < 0x2000) {
                if (rc != UNI_HAL_CRYP_UNSUPPORTED) return 1;
                continue;
            }
            if (rc != UNI_HAL_CRYP_OK || memcmp(tag, tags[mode][tail], sizeof(tag))) return 2;
            if (uni_hal_cryp_decrypt(&m, tag, sizeof(tag)) != UNI_HAL_CRYP_OK) return 3;
            for (size_t i = 0; i < m.size; ++i) {
                if (m.output[i] != (uint8_t)i) return 4;
            }
            if (storage[0] != 0xa5 || storage[m.size + 1] != 0xa5) return 5;
            if (uni_hal_cryp_encrypt(&m, tag, sizeof(tag)) != UNI_HAL_CRYP_OK) return 6;
            tag[0] ^= 1;
            if (uni_hal_cryp_decrypt(&m, tag, sizeof(tag)) != UNI_HAL_CRYP_AUTH_FAILED) return 7;
        }
    }
    return 0;
}

int main(void)
{
    cryp_test_result = run_vectors();
    for (;;) __WFI();
}
