#include "hardware/address_mapped.h"
#include "hardware/clocks.h"
#include "hardware/flash.h"
#include "hardware/gpio.h"
#include "hardware/regs/addressmap.h"
#include "hardware/spi.h"
#include "hardware/structs/qmi.h"
#include "hardware/structs/xip_ctrl.h"
#include "hardware/sync.h"
#include "pico/binary_info.h"
#include "pico/flash.h"
#include "pico/stdlib.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "tlsf/tlsf.h"
#include "rp_pico_alloc.h"

#define PSRAM_CMD_QUAD_END 0xF5
#define PSRAM_CMD_QUAD_ENABLE 0x35
#define PSRAM_CMD_READ_ID 0x9F
#define PSRAM_CMD_RSTEN 0x66
#define PSRAM_CMD_RST 0x99
#define PSRAM_CMD_QUAD_READ 0xEB
#define PSRAM_CMD_QUAD_WRITE 0x38
#define PSRAM_CMD_NOOP 0xFF
#define PSRAM_CMD_LINEAR_TOGGLE 0xC0
#define PSRAM_ID 0x5D

static tlsf_t _mem_heap = NULL;
static pool_t _mem_psram_pool = NULL;
static size_t _psram_size = 0;
static bool _bInitalized = false;

#define PSRAM_LOCATION _u(0x11000000)

static size_t __no_inline_not_in_flash_func(setup_psram)(uint psram_cs_pin)
{
    gpio_set_function(psram_cs_pin, GPIO_FUNC_XIP_CS1);

    size_t psram_size = 0;

    const int max_psram_freq = 133000000;
    const int clock_hz = clock_get_hz(clk_sys);
    int clockDivider = (clock_hz + max_psram_freq - 1) / max_psram_freq;
    if (clockDivider == 1 && clock_hz > 100000000) {
        clockDivider = 2;
    }
    int rxdelay = clockDivider;
    if (clock_hz / clockDivider > 100000000) {
        rxdelay += 1;
    }

    const int clock_period_fs = 1000000000000000ll / clock_hz;
    const int maxSelect = (125 * 1000000) / clock_period_fs;
    const int minDeselect = (18 * 1000000 + (clock_period_fs - 1)) / clock_period_fs - (clockDivider + 1) / 2;

    printf("Max Select: %d, Min Deselect: %d, clock divider: %d\n", maxSelect, minDeselect, clockDivider);

    uint32_t intr_stash = save_and_disable_interrupts();

    qmi_hw->direct_csr = 30 << QMI_DIRECT_CSR_CLKDIV_LSB | QMI_DIRECT_CSR_EN_BITS;

    while ((qmi_hw->direct_csr & QMI_DIRECT_CSR_BUSY_BITS) != 0)
    {
    }

    qmi_hw->direct_csr |= QMI_DIRECT_CSR_ASSERT_CS1N_BITS;
    qmi_hw->direct_tx = QMI_DIRECT_TX_OE_BITS | (QMI_DIRECT_TX_IWIDTH_VALUE_Q << QMI_DIRECT_TX_IWIDTH_LSB) | PSRAM_CMD_QUAD_END;
    while ((qmi_hw->direct_csr & QMI_DIRECT_CSR_BUSY_BITS) != 0)
    {
    }
    (void)qmi_hw->direct_rx;
    qmi_hw->direct_csr &= ~(QMI_DIRECT_CSR_ASSERT_CS1N_BITS);

    qmi_hw->direct_csr |= QMI_DIRECT_CSR_ASSERT_CS1N_BITS;
    uint8_t kgd = 0;
    uint8_t eid = 0;
    for (size_t i = 0; i < 7; i++)
    {
        if (i == 0)
            qmi_hw->direct_tx = PSRAM_CMD_READ_ID;
        else
            qmi_hw->direct_tx = PSRAM_CMD_NOOP;

        while ((qmi_hw->direct_csr & QMI_DIRECT_CSR_TXEMPTY_BITS) == 0)
        {
        }
        while ((qmi_hw->direct_csr & QMI_DIRECT_CSR_BUSY_BITS) != 0)
        {
        }
        if (i == 5)
            kgd = qmi_hw->direct_rx;
        else if (i == 6)
            eid = qmi_hw->direct_rx;
        else
            (void)qmi_hw->direct_rx;
    }

    qmi_hw->direct_csr &= ~(QMI_DIRECT_CSR_ASSERT_CS1N_BITS | QMI_DIRECT_CSR_EN_BITS);
    restore_interrupts(intr_stash);

    if (kgd != PSRAM_ID)
    {
        printf("Invalid PSRAM ID: %x\n", kgd);
        return psram_size;
    }
    printf("Valid PSRAM ID: %x\n", kgd);

    intr_stash = save_and_disable_interrupts();

    qmi_hw->direct_csr = (30 << QMI_DIRECT_CSR_CLKDIV_LSB) | QMI_DIRECT_CSR_EN_BITS;

    while ((qmi_hw->direct_csr & QMI_DIRECT_CSR_BUSY_BITS) != 0)
    {
    }

    for (uint8_t i = 0; i < 4; i++)
    {
        qmi_hw->direct_csr |= QMI_DIRECT_CSR_ASSERT_CS1N_BITS;
        if (i == 0)
            qmi_hw->direct_tx = PSRAM_CMD_RSTEN;
        else if (i == 1)
            qmi_hw->direct_tx = PSRAM_CMD_RST;
        else if (i == 2)
            qmi_hw->direct_tx = PSRAM_CMD_QUAD_ENABLE;
        else
            qmi_hw->direct_tx = PSRAM_CMD_LINEAR_TOGGLE;

        while ((qmi_hw->direct_csr & QMI_DIRECT_CSR_BUSY_BITS) != 0)
        {
        }
        qmi_hw->direct_csr &= ~(QMI_DIRECT_CSR_ASSERT_CS1N_BITS);
        for (size_t j = 0; j < 20; j++)
            asm("nop");

        (void)qmi_hw->direct_rx;
    }

    qmi_hw->direct_csr &= ~(QMI_DIRECT_CSR_ASSERT_CS1N_BITS | QMI_DIRECT_CSR_EN_BITS);

    qmi_hw->m[1].timing =
        (QMI_M1_TIMING_PAGEBREAK_VALUE_1024 << QMI_M1_TIMING_PAGEBREAK_LSB) |
        (1 << QMI_M1_TIMING_COOLDOWN_LSB) | (rxdelay << QMI_M1_TIMING_RXDELAY_LSB) |
        (maxSelect << QMI_M1_TIMING_MAX_SELECT_LSB) |
        (minDeselect << QMI_M1_TIMING_MIN_DESELECT_LSB) |
        (clockDivider << QMI_M1_TIMING_CLKDIV_LSB);

    qmi_hw->m[1].rfmt = (QMI_M1_RFMT_PREFIX_WIDTH_VALUE_Q << QMI_M1_RFMT_PREFIX_WIDTH_LSB) |
                         (QMI_M1_RFMT_ADDR_WIDTH_VALUE_Q << QMI_M1_RFMT_ADDR_WIDTH_LSB) |
                         (QMI_M1_RFMT_SUFFIX_WIDTH_VALUE_Q << QMI_M1_RFMT_SUFFIX_WIDTH_LSB) |
                         (QMI_M1_RFMT_DUMMY_WIDTH_VALUE_Q << QMI_M1_RFMT_DUMMY_WIDTH_LSB) |
                         (QMI_M1_RFMT_DUMMY_LEN_VALUE_24 << QMI_M1_RFMT_DUMMY_LEN_LSB) |
                         (QMI_M1_RFMT_DATA_WIDTH_VALUE_Q << QMI_M1_RFMT_DATA_WIDTH_LSB) |
                         (QMI_M1_RFMT_PREFIX_LEN_VALUE_8 << QMI_M1_RFMT_PREFIX_LEN_LSB) |
                         (QMI_M1_RFMT_SUFFIX_LEN_VALUE_NONE << QMI_M1_RFMT_SUFFIX_LEN_LSB);
    qmi_hw->m[1].rcmd = (PSRAM_CMD_QUAD_READ);
    qmi_hw->m[1].wfmt = (QMI_M1_WFMT_PREFIX_WIDTH_VALUE_Q << QMI_M1_WFMT_PREFIX_WIDTH_LSB) |
                         (QMI_M1_WFMT_ADDR_WIDTH_VALUE_Q << QMI_M1_WFMT_ADDR_WIDTH_LSB) |
                         (QMI_M1_WFMT_SUFFIX_WIDTH_VALUE_Q << QMI_M1_WFMT_SUFFIX_WIDTH_LSB) |
                         (QMI_M1_WFMT_DUMMY_WIDTH_VALUE_Q << QMI_M1_WFMT_DUMMY_WIDTH_LSB) |
                         (QMI_M1_WFMT_DUMMY_LEN_VALUE_NONE << QMI_M1_WFMT_DUMMY_LEN_LSB) |
                         (QMI_M1_WFMT_DATA_WIDTH_VALUE_Q << QMI_M1_WFMT_DATA_WIDTH_LSB) |
                         (QMI_M1_WFMT_PREFIX_LEN_VALUE_8 << QMI_M1_WFMT_PREFIX_LEN_LSB) |
                         (QMI_M1_WFMT_SUFFIX_LEN_VALUE_NONE << QMI_M1_WFMT_SUFFIX_LEN_LSB);
    qmi_hw->m[1].wcmd = (PSRAM_CMD_QUAD_WRITE);

    psram_size = 1024 * 1024;
    uint8_t size_id = eid >> 5;
    if (eid == 0x26 || size_id == 2)
        psram_size *= 8;
    else if (size_id == 0)
        psram_size *= 1;
    else if (size_id == 1)
        psram_size *= 4;

    xip_ctrl_hw->ctrl |= XIP_CTRL_WRITABLE_M1_BITS;
    restore_interrupts(intr_stash);
    printf("PSRAM ID: %x %x\n", kgd, eid);
    return psram_size;
}

static bool rp_pico_alloc_init()
{
    if (_bInitalized)
        return true;

    _mem_heap = NULL;
    _mem_psram_pool = NULL;

    _psram_size = setup_psram(RP2350_XIP_CSI_PIN);
    printf("PSRAM size: %u\n", _psram_size);

    if (_psram_size > 0)
    {
        _mem_heap = tlsf_create_with_pool((void *)PSRAM_LOCATION, _psram_size, 64 * 1024 * 1024);
        _mem_psram_pool = tlsf_get_pool(_mem_heap);
    }

    _bInitalized = true;
    return true;
}

void *rp_mem_malloc(size_t size)
{
    if (!rp_pico_alloc_init())
        return NULL;
    return tlsf_malloc(_mem_heap, size);
}

void rp_mem_free(void *ptr)
{
    if (!rp_pico_alloc_init())
        return;
    tlsf_free(_mem_heap, ptr);
}

void *rp_mem_realloc(void *ptr, size_t size)
{
    if (!rp_pico_alloc_init())
        return NULL;
    return tlsf_realloc(_mem_heap, ptr, size);
}

void *rp_mem_calloc(size_t num, size_t size)
{
    if (!rp_pico_alloc_init())
        return NULL;
    void *ptr = tlsf_malloc(_mem_heap, num * size);
    if (ptr)
        memset(ptr, 0, num * size);
    return ptr;
}

static bool max_free_walker(void *ptr, size_t size, int used, void *user)
{
    size_t *max_size = (size_t *)user;
    if (!used && *max_size < size)
        *max_size = size;
    return true;
}

size_t rp_mem_max_free_size(void)
{
    if (!rp_pico_alloc_init())
        return 0;
    size_t max_free = 0;
    if (_mem_psram_pool)
        tlsf_walk_pool(_mem_psram_pool, max_free_walker, &max_free);
    return max_free;
}
