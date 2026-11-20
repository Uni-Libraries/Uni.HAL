#include <SEGGER_RTT.h>
#include "io/uni_hal_io_tunnel.h"
#include "segger/uni_hal_segger_rtt.h"
#include "timer.h"

static uni_hal_io_context_t *io;
static bool refill;
static unsigned reads, writes, reads_before_tx;

unsigned __real_SEGGER_RTT_Read(unsigned index, void *buffer, unsigned size);
unsigned __wrap_SEGGER_RTT_Read(unsigned index, void *buffer, unsigned size) {
    unsigned const count = __real_SEGGER_RTT_Read(index, buffer, size);
    if (count && refill && ++reads <= 64U) {
        // The debugger writes another byte as soon as the down ring has space.
        uint8_t byte = (uint8_t)(1U + reads);
        CHECK(SEGGER_RTT_WriteDownBuffer(index, &byte, 1U) == 1U);
    }
    return count;
}

unsigned __real_SEGGER_RTT_PutCharSkip(unsigned index, char byte);
unsigned __wrap_SEGGER_RTT_PutCharSkip(unsigned index, char byte) {
    unsigned const count = __real_SEGGER_RTT_PutCharSkip(index, byte);
    if (refill) {
        if (!writes) reads_before_tx = reads;
        if (++writes <= 64U) {
            // A producer queues another byte during the TX phase.
            uint8_t next = (uint8_t)(1U + writes);
            CHECK(uni_hal_io_transmit_data(io, &next, 1U) == 1U);
        }
    }
    return count;
}

static void test_budget(void *unused) {
    (void)unused;
    UNI_HAL_IO_DEFINITION(port, 80U, 8U);
    uni_hal_tim_context_t timer = {};
    uni_hal_segger_context_t ctx = {.io = &port_ctx, .timer = &timer, .buffer_index_up = 1U, .buffer_index_down = 1U};
    char up[80], down[4], prime[3];
    uint8_t const initial[] = {0, 1};
    io = &port_ctx;
    uni_hal_segger_rtt_init();
    CHECK(SEGGER_RTT_ConfigUpBuffer(1U, "up", up, sizeof up, SEGGER_RTT_MODE_NO_BLOCK_SKIP) == 0);
    CHECK(SEGGER_RTT_ConfigDownBuffer(1U, "down", down, sizeof down, SEGGER_RTT_MODE_NO_BLOCK_SKIP) == 0);
    CHECK(uni_hal_segger_rtt_stdio_init(&ctx));
    // Make the two pending RX bytes straddle the physical end of the ring.
    CHECK(SEGGER_RTT_WriteDownBuffer(1U, "abc", 3U) == 3U);
    CHECK(SEGGER_RTT_Read(1U, prime, sizeof prime) == 3U);
    CHECK(SEGGER_RTT_WriteDownBuffer(1U, initial, sizeof initial) == 2U);
    CHECK(uni_hal_io_transmit_data(io, initial, sizeof initial) == 2U);

    refill = true;
    poll_from_irq();
    CHECK(reads_before_tx == 2U && reads == 2U && writes == 2U);
    CHECK(io->stats.rx_received == 2U && io->stats.tx_transmited == 2U);
    refill = false;
    poll_from_irq();
    CHECK(io->stats.rx_received == 4U && io->stats.tx_transmited == 4U);
    CHECK(!io->stats.rx_overrun && !io->stats.tx_dropped);
    uint8_t rx[4], tx[4];
    CHECK(uni_hal_io_receive_data(io, rx, sizeof rx, 0U) == sizeof rx);
    CHECK(SEGGER_RTT_ReadUpBuffer(1U, tx, sizeof tx) == sizeof tx);
    for (unsigned i = 0; i < 4U; ++i) CHECK(rx[i] == i && tx[i] == i);

    // Input routed to the same transmit queue during the RX phase still leaves in this poll.
    uni_hal_io_tunnel_context_t tunnel = {};
    CHECK(uni_hal_io_tunnel_init(&tunnel, io, io, false));
    CHECK(SEGGER_RTT_WriteDownBuffer(1U, "T", 1U) == 1U);
    poll_from_irq();
    CHECK(SEGGER_RTT_ReadUpBuffer(1U, tx, 1U) == 1U && tx[0] == 'T');
    CHECK(io->stats.rx_received == 4U && io->stats.tx_transmited == 5U);
    finish(0U);
}

int main(void) {
    CHECK(xTaskCreate(test_budget, "rtt", 512U, nullptr, 1U, nullptr) == pdPASS);
    vTaskStartScheduler();
    finish(__LINE__);
}
