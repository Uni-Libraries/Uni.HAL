#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#if defined(UNI_HAL_TARGET_MCU_STM32H753)
#define UNI_HAL_HAS_CRYP 1

typedef enum {
    UNI_HAL_CRYP_AES_GCM,
    UNI_HAL_CRYP_AES_CCM
} uni_hal_cryp_mode_e;

typedef enum {
    UNI_HAL_CRYP_OK,
    UNI_HAL_CRYP_INVALID,
    UNI_HAL_CRYP_UNSUPPORTED,
    UNI_HAL_CRYP_BUSY,
    UNI_HAL_CRYP_TIMEOUT,
    UNI_HAL_CRYP_AUTH_FAILED
} uni_hal_cryp_status_e;

/** One AES-GCM or AES-CCM message using an AES-128, AES-192 or AES-256 key.
 * All sizes are bytes. Buffers can be unaligned and input and output may be
 * identical. A zero-length payload or AAD may have a null buffer.
 */
typedef struct {
    uni_hal_cryp_mode_e mode;
    const uint8_t* key;
    size_t key_len;
    const uint8_t* nonce;
    size_t nonce_len;
    const uint8_t* aad;
    size_t aad_len;
    const uint8_t* input;
    uint8_t* output;
    size_t size;
} uni_hal_cryp_message_t;

/** Encrypt and authenticate one message. Reserve each nonce for a single
 * encryption with its key. GCM takes a 12-byte IV and a 4..16-byte tag.
 * CCM takes a 7..13-byte nonce and an even 4..16-byte tag.
 *
 * BUSY (a concurrent call owns CRYP) and UNSUPPORTED leave outputs untouched.
 * On STM32H753 silicon before revision B, partial final payload blocks require
 * a software fallback. The driver owns the peripheral during a call.
 */
uni_hal_cryp_status_e uni_hal_cryp_encrypt(const uni_hal_cryp_message_t* message,
                                           uint8_t* tag, size_t tag_len);

/** Decrypt and verify the detached tag. Use the output only after OK. */
uni_hal_cryp_status_e uni_hal_cryp_decrypt(const uni_hal_cryp_message_t* message,
                                           const uint8_t* tag, size_t tag_len);
#endif

#ifdef __cplusplus
}
#endif
