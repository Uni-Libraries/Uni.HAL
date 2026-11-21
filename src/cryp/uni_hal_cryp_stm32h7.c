#include "cryp/uni_hal_cryp.h"

#if defined(UNI_HAL_HAS_CRYP)
#include <string.h>
#include <stm32h7xx.h>

#include "core/uni_hal_core.h"

enum { block_size = 16 };
static bool owned;

/* Poll individual peripheral transitions, including before the scheduler and
 * tick interrupt are running. Normal transitions take a few hundred cycles. */
static bool wait_bits(volatile const uint32_t* reg, uint32_t mask, uint32_t expected)
{
    for (uint32_t polls = 0; polls < 100000; ++polls) {
        if ((*reg & mask) == expected) {
            return true;
        }
    }
    return false;
}

static uint32_t read_be32(const uint8_t* bytes)
{
    uint32_t word;
    memcpy(&word, bytes, sizeof(word));
    return __REV(word);
}

static void write_be32(uint8_t* bytes, uint32_t word)
{
    word = __REV(word);
    memcpy(bytes, &word, sizeof(word));
}

/* DATATYPE=0 presents the FIFO, key registers and IV as big-endian words.
 * The public buffers carry network-order bytes and need no special alignment. */
static void write_block(const uint8_t block[block_size])
{
    for (size_t i = 0; i < block_size; i += 4) {
        CRYP->DIN = read_be32(block + i);
    }
}

static bool read_block(uint8_t block[block_size])
{
    if (!wait_bits(&CRYP->SR, CRYP_SR_OFNE, CRYP_SR_OFNE)) {
        return false;
    }
    for (size_t i = 0; i < block_size; i += 4) {
        write_be32(block + i, CRYP->DOUT);
    }
    return true;
}

static void load_key(const uint8_t* key, size_t length)
{
    volatile uint32_t* const registers[] = {
        &CRYP->K0LR, &CRYP->K0RR, &CRYP->K1LR, &CRYP->K1RR,
        &CRYP->K2LR, &CRYP->K2RR, &CRYP->K3LR, &CRYP->K3RR
    };
    /* Short keys are right-aligned in the eight key registers. */
    for (size_t i = 0; i < length / 4; ++i) {
        *registers[8 - length / 4 + i] = read_be32(key + 4 * i);
    }
}

static void start_phase(uint32_t control)
{
    CRYP->CR &= ~CRYP_CR_CRYPEN;
    CRYP->CR = control;
    CRYP->CR = control | CRYP_CR_CRYPEN;
}

static bool acquire(bool* clock_was_enabled)
{
    uint32_t mask = uni_hal_core_irq_pause();
    if (owned) {
        uni_hal_core_irq_resume(mask);
        return false;
    }
    owned = true;
    *clock_was_enabled = (RCC->AHB2ENR & RCC_AHB2ENR_CRYPEN) != 0;
    RCC->AHB2ENR |= RCC_AHB2ENR_CRYPEN;
    (void)RCC->AHB2ENR; /* RCC clock-enable delay before accessing CRYP. */
    uni_hal_core_irq_resume(mask);
    return true;
}

static void release(bool clock_was_enabled)
{
    uint32_t mask = uni_hal_core_irq_pause();
    /* A peripheral reset erases the key registers, FIFOs and GHASH/CBC-MAC state,
     * including when a hardware transition timed out. */
    RCC->AHB2RSTR |= RCC_AHB2RSTR_CRYPRST;
    RCC->AHB2RSTR &= ~RCC_AHB2RSTR_CRYPRST;
    if (!clock_was_enabled) {
        RCC->AHB2ENR &= ~RCC_AHB2ENR_CRYPEN;
    }
    owned = false;
    uni_hal_core_irq_resume(mask);
}

static void encode_length(uint8_t* end, size_t length, size_t field_size)
{
    for (size_t i = 0; i < field_size; ++i) {
        *--end = (uint8_t)length;
        length >>= 8;
    }
}

static bool initialize(const uni_hal_cryp_message_t* m, size_t tag_len, uint32_t control,
                       uint8_t counter[block_size])
{
    uint8_t b0[block_size] = {0};
    if (m->mode == UNI_HAL_CRYP_AES_GCM) {
        memcpy(counter, m->nonce, 12);
        counter[15] = 2; /* inc32(J0), the first GCM payload counter. */
    } else {
        size_t l = 15 - m->nonce_len;
        counter[0] = (uint8_t)(l - 1);
        memcpy(counter + 1, m->nonce, m->nonce_len);
        /* B0 and A1 share the nonce. Only B0 encodes the tag and message lengths. */
        memcpy(b0, counter, block_size);
        b0[0] |= (m->aad_len ? 0x40u : 0u) | (uint8_t)(((tag_len - 2) / 2) << 3);
        encode_length(b0 + block_size, m->size, l);
        counter[15] = 1;
    }
    CRYP->CR = control;
    load_key(m->key, m->key_len);
    CRYP->IV0LR = read_be32(counter);
    CRYP->IV0RR = read_be32(counter + 4);
    CRYP->IV1LR = read_be32(counter + 8);
    CRYP->IV1RR = read_be32(counter + 12);
    CRYP->CR |= CRYP_CR_CRYPEN;
    if (m->mode == UNI_HAL_CRYP_AES_CCM) {
        write_block(b0);
    }
    return wait_bits(&CRYP->CR, CRYP_CR_CRYPEN, 0);
}

static bool authenticate_header(const uni_hal_cryp_message_t* m, uint32_t control)
{
    if (!m->aad_len) {
        return true;
    }
    uint8_t block[block_size] = {0};
    size_t prefix = 0;
    if (m->mode == UNI_HAL_CRYP_AES_CCM) {
        /* SP 800-38C A.2.2. On this 32-bit target AAD length fits in four bytes. */
        if (m->aad_len >= 0xff00) {
            block[0] = 0xff;
            block[1] = 0xfe;
            write_be32(block + 2, m->aad_len);
            prefix = 6;
        } else {
            block[0] = (uint8_t)(m->aad_len >> 8);
            block[1] = (uint8_t)m->aad_len;
            prefix = 2;
        }
    }
    start_phase(control | CRYP_CR_GCM_CCMPH_0);
    for (size_t offset = 0; offset < m->aad_len;) {
        size_t n = m->aad_len - offset;
        if (n > block_size - prefix) {
            n = block_size - prefix;
        }
        memcpy(block + prefix, m->aad + offset, n);
        memset(block + prefix + n, 0, block_size - prefix - n);
        write_block(block);
        if (!wait_bits(&CRYP->SR, CRYP_SR_IFEM, CRYP_SR_IFEM)) {
            return false;
        }
        offset += n;
        prefix = 0;
    }
    return wait_bits(&CRYP->SR, CRYP_SR_BUSY, 0);
}

static bool transform_payload(const uni_hal_cryp_message_t* m, bool decrypt, uint32_t control)
{
    control |= CRYP_CR_GCM_CCMPH_1;
    start_phase(control);
    for (size_t offset = 0; offset < m->size;) {
        uint8_t block[block_size] = {0};
        size_t n = m->size - offset;
        if (n > block_size) {
            n = block_size;
        }
        /* When CRYP authenticates the output (GCM encrypt or CCM decrypt),
         * tell it which bytes of the last AES block to exclude from the MAC. */
        if (n < block_size && ((m->mode == UNI_HAL_CRYP_AES_CCM) == decrypt)) {
            start_phase(control | ((block_size - n) << CRYP_CR_NPBLB_Pos));
        }
        memcpy(block, m->input + offset, n);
        write_block(block);
        if (!read_block(block)) {
            return false;
        }
        memcpy(m->output + offset, block, n);
        offset += n;
    }
    return true;
}

static bool calculate_tag(const uni_hal_cryp_message_t* m, uint32_t control,
                          uint8_t counter[block_size], uint8_t tag[block_size])
{
    if (m->mode == UNI_HAL_CRYP_AES_GCM) {
        /* GCM's final input consists of the two 64-bit big-endian bit lengths. */
        uint64_t aad_bits = (uint64_t)m->aad_len * 8;
        uint64_t data_bits = (uint64_t)m->size * 8;
        write_be32(counter, (uint32_t)(aad_bits >> 32));
        write_be32(counter + 4, (uint32_t)aad_bits);
        write_be32(counter + 8, (uint32_t)(data_bits >> 32));
        write_be32(counter + 12, (uint32_t)data_bits);
    } else {
        counter[15] = 0; /* A0 encrypts the CCM authentication value. */
    }
    start_phase((control & ~CRYP_CR_ALGODIR) | CRYP_CR_GCM_CCMPH);
    write_block(counter);
    return read_block(tag);
}

static uni_hal_cryp_status_e process(const uni_hal_cryp_message_t* m, uint8_t* tag_out,
                                     const uint8_t* expected_tag, size_t tag_len)
{
    if (!m || !m->key || !m->nonce || (!tag_out && !expected_tag) ||
        (m->size && (!m->input || !m->output)) || (m->aad_len && !m->aad) ||
        (m->key_len != 16 && m->key_len != 24 && m->key_len != 32) ||
        tag_len < 4 || tag_len > 16) {
        return UNI_HAL_CRYP_INVALID;
    }
    uint32_t control;
    switch (m->mode) {
        case UNI_HAL_CRYP_AES_GCM:
            if (m->nonce_len != 12) {
                return UNI_HAL_CRYP_UNSUPPORTED;
            }
            control = CRYP_CR_ALGOMODE_AES_GCM;
            break;
        case UNI_HAL_CRYP_AES_CCM:
            if (m->nonce_len < 7 || m->nonce_len > 13 || (tag_len & 1)) {
                return UNI_HAL_CRYP_INVALID;
            }
            size_t l = 15 - m->nonce_len;
            if (l < sizeof(size_t) && m->size >= ((size_t)1 << (8 * l))) {
                return UNI_HAL_CRYP_INVALID;
            }
            control = CRYP_CR_ALGOMODE_AES_CCM;
            break;
        default:
            return UNI_HAL_CRYP_UNSUPPORTED;
    }
    /* Before revision B the H753 has no NPBLB field (RM0433, CRYP_CR). */
    if ((m->size % block_size) && (DBGMCU->IDCODE >> DBGMCU_IDCODE_REV_ID_Pos) < 0x2000) {
        return UNI_HAL_CRYP_UNSUPPORTED;
    }
    bool clock_was_enabled;
    if (!acquire(&clock_was_enabled)) {
        return UNI_HAL_CRYP_BUSY;
    }
    bool decrypt = expected_tag != nullptr;
    if (decrypt) {
        control |= CRYP_CR_ALGODIR;
    }
    control |= ((m->key_len - 16) / 8) << CRYP_CR_KEYSIZE_Pos;
    uint8_t counter[block_size] = {0};
    uint8_t tag[block_size];
    bool ok = initialize(m, tag_len, control, counter) &&
              authenticate_header(m, control) &&
              transform_payload(m, decrypt, control) &&
              calculate_tag(m, control, counter, tag);
    release(clock_was_enabled);
    if (!ok) {
        return UNI_HAL_CRYP_TIMEOUT;
    }
    if (decrypt) {
        /* Compare every authenticated byte before releasing a success result. */
        uint32_t diff = 0;
        for (size_t i = 0; i < tag_len; ++i) {
            diff |= tag[i] ^ expected_tag[i];
        }
        return diff ? UNI_HAL_CRYP_AUTH_FAILED : UNI_HAL_CRYP_OK;
    }
    memcpy(tag_out, tag, tag_len);
    return UNI_HAL_CRYP_OK;
}

uni_hal_cryp_status_e uni_hal_cryp_encrypt(const uni_hal_cryp_message_t* message,
                                           uint8_t* tag, size_t tag_len)
{
    return process(message, tag, nullptr, tag_len);
}

uni_hal_cryp_status_e uni_hal_cryp_decrypt(const uni_hal_cryp_message_t* message,
                                           const uint8_t* tag, size_t tag_len)
{
    return process(message, nullptr, tag, tag_len);
}
#endif
