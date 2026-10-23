/******************************************************************************
 * File Name    : rs_ota_fw_update_over_ble_hal_ra6e2.c
 * Description  : RA6E2 + DA14531 implementation of the ota_fw_update_over_ble
 *                HAL (see hal/rs_ota_fw_update_over_ble_hal.h).
 *
 *                This is the ONLY translation unit of the RS that includes FSP /
 *                BLE-stack headers (§9.2). It implements:
 *                  - the code-flash staging store (FSP r_flash_hp),
 *                  - the DA14531 BLE/SUOTA transport seam,
 *                  - the diagnostic report sink (application callback).
 *
 *                NO PART OF THE RS MAY TOUCH SEGGER RTT - not even this file.
 *                Diagnostics are formatted here and handed to the application
 *                through cfg->p_callback; choosing a transport (RTT, UART, file,
 *                nothing at all) is the application's business, in src/app.
 *
 *                It reuses the FSP flash instance g_flash0 (opened at runtime by
 *                the VEE middleware during RM_BLE_ABS_Open). The FACI allows only
 *                one flash operation at a time, so sharing the single instance is
 *                the sanctioned pattern; VEE uses data flash, this module uses
 *                code flash, and operations are serialised by the BLE task context.
 ******************************************************************************/
#include "rs_ota_fw_update_over_ble_hal.h"
#include "rs_ota_fw_update_over_ble_deps.h"
#include "rs_ota_fw_update_over_ble_board_ra6e2.h"
#include "ble_thread.h"     /* g_flash0_ctrl (already opened by VEE) + r_flash_hp.h */
#include "semphr.h"
#include "event_groups.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

/* Board/target geometry (from hal/rs_ota_fw_update_over_ble_deps.h).
 * Local aliases keep the flash calls readable. */
#define STAGE_ADDR       RS_OTA_FW_UPDATE_OVER_BLE_CFG_STAGE_ADDR
#define STAGE_SIZE       RS_OTA_FW_UPDATE_OVER_BLE_CFG_STAGE_SIZE
#define PROGRAM_UNIT     RS_OTA_FW_UPDATE_OVER_BLE_CFG_PROGRAM_UNIT
#define ERASE_BLOCK_SIZE RS_OTA_FW_UPDATE_OVER_BLE_CFG_ERASE_BLOCK_SIZE

/* Bound for one formatted report line handed to the application sink. Longer
 * lines are truncated (still NUL-terminated). */
#define RS_FW_LOG_BUF_LEN  (256U)

/* Lines produced BEFORE the application registers its callback are parked here
 * and replayed on registration (see the log-sink block below). Eight short lines
 * cover boot-time diagnostics; the cost is fixed BSS, so it is deliberately
 * modest on a 40 KB-SRAM part. */
#define RS_FW_LOG_EARLY_LINES  (8U)
#define RS_FW_LOG_EARLY_LEN    (128U)

/* ---- reference board configuration object ---------------------------------- *
 * A customer adapting to a different board copies and edits this file (§8, §13.10). */
const rs_ota_fw_update_over_ble_cfg_t g_rs_ota_fw_update_over_ble_cfg_ra6e2 =
{
    .version    = 1U,
    .size       = (uint16_t) sizeof(rs_ota_fw_update_over_ble_cfg_t),
    .mode       = RS_OTA_FW_UPDATE_OVER_BLE_EXECUTION_MODE_NON_BLOCKING,
    .stage_addr = STAGE_ADDR,
    .stage_size = STAGE_SIZE,
    .p_callback = NULL,
};

/* ---- staging store state (static; no dynamic allocation, §12 rule 5) ------- */
static uint32_t s_offset;                     /* bytes committed to flash   */
static uint8_t  s_page[PROGRAM_UNIT];         /* program-unit write buffer  */
static uint32_t s_page_fill;                  /* valid bytes in s_page      */

/* ---- transport state ------------------------------------------------------- */
static rs_ota_fw_update_over_ble_hal_callbacks_t s_cb;
/* Written by the thread that opens/closes the transport, read by the BLE task in
 * the feed_* relays. */
static volatile bool                             s_transport_open;

/******************************************************************************
 * Diagnostic report sink.
 *
 * The RS stays transport-agnostic AND SEGGER-free: it formats each line into a
 * static buffer here and forwards the finished string to the application-supplied
 * callback (registered from rs_ota_fw_update_over_ble_Start via cfg->p_callback /
 * cfg->p_context). Only src/app knows what that string is written to - in this
 * demo, SEGGER RTT channel 0 via OTA_LOG(). There is no UART log path: SCI0
 * carries the DA14531 GTL link.
 *
 * The buffer is static (no dynamic allocation, §12 rule 5) and safe to share
 * because RS logs are serialised on the BLE task.
 *
 * WHATEVER SINK THE APPLICATION PLUGS IN MUST NOT BLOCK. Most of these calls are
 * made from the BLE task: a blocking write with no reader attached stalls
 * R_BLE_Execute(), the DA14531 GTL/UART link stops being serviced, and the phone
 * SUOTA app times out (~20 s) and disconnects. Drop output, never wait.
 *
 * EARLY LINES: the RS reports before the application has registered anything
 * (dependency checks, flash-guard/DTC bring-up). Rather than lose them, they are
 * parked in s_early[] and replayed, in order, the moment a callback arrives. The
 * park is one-shot: once drained it is never refilled, so a later Stop()/Start()
 * cycle does not resurrect stale lines.
 ******************************************************************************/
static rs_ota_fw_update_over_ble_cb_t s_cb_sink;
static void *                         s_cb_context;

/* Early-line park. Written only before a callback exists (single-threaded
 * start-up path) and read once from ..._hal_set_cb(). */
static char        s_early[RS_FW_LOG_EARLY_LINES][RS_FW_LOG_EARLY_LEN];
static rs_status_t s_early_status[RS_FW_LOG_EARLY_LINES];
static uint32_t    s_early_count;     /* lines currently parked          */
static uint32_t    s_early_dropped;   /* lines lost because it was full  */
static bool        s_early_drained;   /* park is closed for good         */

void rs_ota_fw_update_over_ble_hal_set_cb(
    rs_ota_fw_update_over_ble_cb_t cb,
    void *                         p_context)
{
    s_cb_sink    = cb;
    s_cb_context = p_context;

    if ((NULL == cb) || s_early_drained)
    {
        return;
    }

    /* Replay what was reported before the application could listen. */
    s_early_drained = true;

    for (uint32_t i = 0U; i < s_early_count; i++)
    {
        cb(s_early_status[i], s_early[i], p_context);
    }

    if (s_early_dropped > 0U)
    {
        static char s_note[64];
        (void) snprintf(s_note, sizeof(s_note),
                        "[FWACQ] %u early report line(s) dropped\r\n",
                        (unsigned) s_early_dropped);
        cb(RS_STATUS_SUCCESS, s_note, p_context);
    }

    s_early_count = 0U;
}

/* Shared back-end for both log helpers: format @p p_fmt/@p args into the static
 * buffer and forward the line to the registered callback. With no callback yet,
 * the line is parked for replay (or counted as dropped if the park is full). */
static void rs_ota_fw_update_over_ble_hal_vlog(rs_status_t status,
                                               const char * p_fmt,
                                               va_list      args)
{
    if (NULL == s_cb_sink)
    {
        if (s_early_drained)
        {
            return;   /* application ran and deregistered: nothing to do */
        }

        if (s_early_count < RS_FW_LOG_EARLY_LINES)
        {
            (void) vsnprintf(s_early[s_early_count], RS_FW_LOG_EARLY_LEN, p_fmt, args);
            s_early_status[s_early_count] = status;
            s_early_count++;
        }
        else
        {
            s_early_dropped++;
        }
        return;
    }

    static char s_buf[RS_FW_LOG_BUF_LEN];

    (void) vsnprintf(s_buf, sizeof(s_buf), p_fmt, args);
    s_cb_sink(status, s_buf, s_cb_context);
}

void rs_ota_fw_update_over_ble_hal_log(const char * p_fmt, ...)
{
    va_list args;
    va_start(args, p_fmt);
    rs_ota_fw_update_over_ble_hal_vlog(RS_STATUS_SUCCESS, p_fmt, args);
    va_end(args);
}

void rs_ota_fw_update_over_ble_hal_logs(rs_status_t status, const char * p_fmt, ...)
{
    va_list args;
    va_start(args, p_fmt);
    rs_ota_fw_update_over_ble_hal_vlog(status, p_fmt, args);
    va_end(args);
}

/******************************************************************************
 * ---- Flash guard: lossless DA14531 UART capture across a code-flash P/E -----
 *
 * On single-bank RA6E2 the CPU cannot fetch from code flash while the FACI is in
 * P/E mode, so every interrupt must be masked for the whole operation (tens of ms
 * for a 128-byte write, up to ~240 ms for a 32 KB block erase).
 *
 * SCI0 carries the DA14531 GTL link and has NO FIFO (SCI_UART_CFG_FIFO_SUPPORT is
 * 0) and NO hardware flow control, so the hardware can hold exactly ONE unread
 * byte (~87 us at 115200 baud). Anything the DA sends during the blackout used to
 * be dropped, and since GTL is a length-prefixed stream parsed byte-by-byte in the
 * RXI ISR (R_BLE_GTL_UartCallback) with no resync, a single lost byte desynced the
 * parser for good: the next notification failed with BLE_ERR_NOT_FOUND (0x000D),
 * the disconnect event was never decoded, advertising never restarted, and the
 * device was bricked until reset.
 *
 * The fix does not rely on timing at all. Around each P/E operation the SCI0 RXI
 * event is temporarily routed to the DTC, which copies SCI0.RDR into the RAM
 * buffer below. The DTC reads its vector table (linker section
 * .fsp_dtc_vector_table, placed in RAM) and its transfer info from SRAM and writes
 * to SRAM, so it never touches code flash and keeps running while the CPU is
 * frozen. Afterwards the captured bytes are pushed back through
 * R_BLE_GTL_UartCallback() in order, so the GTL parser sees one unbroken stream.
 *
 * Ordering is the subtle part and is handled explicitly in ..._guard_end():
 * the live RXI vector stays masked until the replay is finished, otherwise a byte
 * received after the erase would overtake the older captured ones.
 ******************************************************************************/

/* Worst case is one 32 KB block erase (~240 ms) at 115200 baud 8N1 = 11520 B/s,
 * i.e. ~2765 bytes. Sized above the physical maximum on purpose: an overflow is
 * not a tuning problem, it means something else is badly wrong. */
#define RS_OTA_FLASH_GUARD_CAPTURE_BYTES    (3072U)

static uint8_t  s_rx_capture[RS_OTA_FLASH_GUARD_CAPTURE_BYTES];
static bool     s_flash_guard_ready;      /* DTC opened successfully          */
static uint32_t s_flash_guard_captured;   /* bytes replayed by the last guard */

/* Set when a guard could not replay everything it caught: the GTL stream has a
 * hole and its parser cannot resync on its own. Acted on by ..._link_poll()
 * (BLE task, outside any BLE callback), which restarts the DA14531 link. */
static volatile bool s_link_resync;

/* Idempotent: the DTC is opened once, on the first guarded flash operation.
 * R_DTC_Open() only powers the block and publishes the transfer info pointer - it
 * does NOT set IELSR.DTCE - so the normal per-byte RXI path stays untouched until
 * a guard actually arms the transfer. */
static void rs_ota_flash_guard_init(void)
{
    if (s_flash_guard_ready)
    {
        return;
    }

    fsp_err_t err = R_DTC_Open(&g_transfer0_ctrl, &g_transfer0_cfg);

    if ((FSP_SUCCESS == err) || (FSP_ERR_ALREADY_OPEN == err))
    {
        s_flash_guard_ready = true;
    }
    else
    {
        /* No capture means every flash operation would drop DA14531 bytes, so
         * they are refused (see ..._guard_begin). Retried on the next one. */
        rs_ota_fw_update_over_ble_hal_logs(RS_STATUS_ERR_HARDWARE,
            "[FWACQ] DTC open failed (err=%d) - flash operation refused\r\n",
            (int) err);
    }
}

/* Arm the capture, then mask interrupts. The DTC is enabled BEFORE the mask on
 * purpose: from the moment DTCE is set the CPU interrupt is suppressed (the
 * transfer info uses TRANSFER_IRQ_END), so no byte can be delivered twice, and any
 * byte already latched is still serviced normally by the ISR beforehand.
 *
 * Returns false - with interrupts still ENABLED and nothing armed - when the
 * capture is unavailable. The caller must then skip the flash operation and
 * report it as failed (and must not call ..._guard_end). Running it unguarded
 * would drop the DA14531 bytes sent during the blackout and desync the GTL
 * link; failing this one write/erase instead fails the transfer cleanly, with
 * the link still in sync to report it to the client. */
static bool rs_ota_flash_guard_begin(void)
{
    rs_ota_flash_guard_init();

    s_flash_guard_captured = 0U;

    if (!s_flash_guard_ready)
    {
        return false;   /* already reported by ..._guard_init() */
    }

    /* R_DTC_Reset() disables, waits for idle, writes src/dest/length and
     * re-enables in one call - and it is the only correct entry point, since
     * the generated transfer info carries NULL pointers that R_DTC_Enable()
     * would assert on. */
    fsp_err_t err = R_DTC_Reset(&g_transfer0_ctrl,
                                (void const *) &R_SCI0->RDR,
                                s_rx_capture,
                                (uint16_t) RS_OTA_FLASH_GUARD_CAPTURE_BYTES);
    if (FSP_SUCCESS != err)
    {
        rs_ota_fw_update_over_ble_hal_logs(RS_STATUS_ERR_HARDWARE,
            "[FWACQ] DTC arm failed (err=%d) - flash operation refused\r\n",
            (int) err);
        return false;
    }

    __disable_irq();
    return true;
}

/* Stop the capture, restore the live path and replay what was caught. Only
 * called after ..._guard_begin() returned true. */
static void rs_ota_flash_guard_end(void)
{
    uint32_t              captured    = 0U;
    fsp_err_t             info_err    = FSP_ERR_NOT_ENABLED;
    transfer_properties_t props       = {0};

    /* If the DTC could not be stopped it may still be writing into
     * s_rx_capture, so neither the count nor the buffer is stable: replay
     * nothing rather than feed the parser a racing snapshot. */
    fsp_err_t disable_err = R_DTC_Disable(&g_transfer0_ctrl);

    if (FSP_SUCCESS == disable_err)
    {
        info_err = R_DTC_InfoGet(&g_transfer0_ctrl, &props);
        if (FSP_SUCCESS == info_err)
        {
            captured = RS_OTA_FLASH_GUARD_CAPTURE_BYTES - props.transfer_length_remaining;
        }
    }

    /* Keep the live RXI masked (but NOT cleared - a request pending here still has
     * its byte waiting in RDR) until the replay is done, so the parser receives
     * the captured bytes before any newer one. */
    NVIC_DisableIRQ(g_uart0_cfg.rxi_irq);

    __enable_irq();

    for (uint32_t i = 0U; i < captured; i++)
    {
        uart_callback_args_t args =
        {
            .channel   = g_uart0_cfg.channel,
            .event     = UART_EVENT_RX_CHAR,
            .data      = (uint32_t) s_rx_capture[i],
            .p_context = NULL,
        };

        R_BLE_GTL_UartCallback(&args);
    }

    NVIC_EnableIRQ(g_uart0_cfg.rxi_irq);

    s_flash_guard_captured = captured;

    /* From here on the flash operation has happened and cannot be undone, but
     * the GTL stream has a hole whenever the capture was not (fully) replayed.
     * The parser never resyncs on its own, so schedule a link restart.
     * Logged here, not at the failure point: interrupts were still masked there. */
    if ((FSP_SUCCESS != disable_err) || (FSP_SUCCESS != info_err))
    {
        s_link_resync = true;
        rs_ota_fw_update_over_ble_hal_logs(RS_STATUS_ERR_HARDWARE,
            "[FWACQ] DTC stop/read failed (disable=%d info=%d) - capture lost, restarting BLE link\r\n",
            (int) disable_err, (int) info_err);
    }
    else if (captured >= RS_OTA_FLASH_GUARD_CAPTURE_BYTES)
    {
        /* Physically impossible at line rate for a single P/E operation: the DTC
         * ran out of room, so bytes were still lost. */
        s_link_resync = true;
        rs_ota_fw_update_over_ble_hal_logs(RS_STATUS_ERR_HARDWARE,
            "[FWACQ] UART capture OVERFLOW (%u bytes) - restarting BLE link\r\n",
            (unsigned) captured);
    }
    else
    {
        /* every captured byte was replayed in order */
    }
}

/******************************************************************************
 * Program the current page to code flash at STAGE_ADDR + s_offset.
 * Runs inside the flash guard: interrupts are masked for the write, and any
 * DA14531 traffic that arrives meanwhile is captured by the DTC and replayed.
 ******************************************************************************/
static fsp_err_t rs_ota_fw_update_over_ble_hal_program_page(void)
{
    fsp_err_t err = FSP_ERR_NOT_ENABLED;   /* UART capture unavailable: not written */

    if (rs_ota_flash_guard_begin())
    {
        err = R_FLASH_HP_Write(&g_flash0_ctrl,
                               (uint32_t) s_page,
                               STAGE_ADDR + s_offset,
                               PROGRAM_UNIT);
        rs_ota_flash_guard_end();
    }

    if (FSP_SUCCESS == err)
    {
        s_offset   += PROGRAM_UNIT;
        s_page_fill = 0;
    }
    else
    {
        rs_ota_fw_update_over_ble_hal_logs(RS_STATUS_ERR_HARDWARE,
                                           "[FWACQ] write err=%d @+%u\r\n",
                                           (int) err, (unsigned) s_offset);
    }
    return err;
}

/******************************************************************************
 * Erase ONE erase block of the staging window.
 *
 * A code-flash erase masks every interrupt for the whole operation (one 32 KB
 * block costs tens of ms up to ~240 ms), which used to starve the DA14531
 * GTL/UART link and desync its parser for good - see the flash-guard comment
 * block above. The operation now runs inside that guard, so DA14531 bytes are
 * captured by the DTC and replayed afterwards instead of being lost.
 *
 * Erasing is still split per block: it keeps each guarded window (and therefore
 * the capture buffer) small, and lets the caller run R_BLE_Execute() in between so
 * the client is serviced promptly. It is no longer what makes the erase SAFE.
 *
 * @param[in] index  Block index inside the staging window (0 .. block_count-1).
 *                   Index 0 also resets the write offset / page buffer.
 ******************************************************************************/
rs_status_t rs_ota_fw_update_over_ble_hal_stage_erase_block(uint32_t index)
{
    fsp_err_t err;
    bool      opened_here = false;

    if (index >= (STAGE_SIZE / ERASE_BLOCK_SIZE))
    {
        return RS_STATUS_ERR_ASSERTION;
    }

    if (0U == index)
    {
        /* Start of a new download: discard any previous staging progress. */
        s_offset    = 0;
        s_page_fill = 0;
    }

    /* Normally already open (VEE opens g_flash0 during RM_BLE_ABS_Open). Only
     * close it again if this call is what opened it. */
    err = R_FLASH_HP_Open(&g_flash0_ctrl, &g_flash0_cfg);
    if (FSP_SUCCESS == err)
    {
        opened_here = true;
    }
    else if (FSP_ERR_ALREADY_OPEN == err)
    {
        err = FSP_SUCCESS;
    }
    else
    {
        /* keep err for the report below */
    }

    if (FSP_SUCCESS == err)
    {
        if (rs_ota_flash_guard_begin())
        {
            err = R_FLASH_HP_Erase(&g_flash0_ctrl,
                                   STAGE_ADDR + (index * ERASE_BLOCK_SIZE),
                                   1U);
            rs_ota_flash_guard_end();
        }
        else
        {
            err = FSP_ERR_NOT_ENABLED;   /* UART capture unavailable: not erased */
        }
    }

    if (opened_here)
    {
        R_FLASH_HP_Close(&g_flash0_ctrl);
    }

    rs_ota_fw_update_over_ble_hal_logs((FSP_SUCCESS == err) ? RS_STATUS_SUCCESS
                                                        : RS_STATUS_ERR_HARDWARE,
                                   "[FWACQ] erase block %u @0x%08X -> err=%d (rx rescued=%u)\r\n",
                                   (unsigned) index,
                                   (unsigned) (STAGE_ADDR + (index * ERASE_BLOCK_SIZE)),
                                   (int) err,
                                   (unsigned) s_flash_guard_captured);

    return (FSP_SUCCESS == err) ? RS_STATUS_SUCCESS : RS_STATUS_ERR_HARDWARE;
}

uint32_t rs_ota_fw_update_over_ble_hal_stage_block_count(void)
{
    return STAGE_SIZE / ERASE_BLOCK_SIZE;
}

/******************************************************************************
 * Prepare the staging window for a NEW download: erase all of it and reset the
 * offset.
 *
 * WHEN: once per download, NOT at boot - erasing at every start-up destroys an
 * already staged image and cycles the flash for nothing. Slot 2 also holds the
 * fallback image until the next download starts, so it must keep its contents.
 *
 * Each block is erased through the flash guard, so this is safe with a live
 * DA14531 link. It does however hold the CPU for the sum of all block erase times
 * (~0.7 s for 96 KB) with no chance to run R_BLE_Execute(), which can delay the
 * client; prefer driving rs_ota_fw_update_over_ble_hal_stage_erase_block() one
 * block per BLE main-loop pass while connected (see ble/app_main.c).
 ******************************************************************************/
rs_status_t rs_ota_fw_update_over_ble_hal_stage_open(void)
{
    const uint32_t blocks = STAGE_SIZE / ERASE_BLOCK_SIZE;
    rs_status_t    st     = RS_STATUS_SUCCESS;

    for (uint32_t i = 0U; i < blocks; i++)
    {
        st = rs_ota_fw_update_over_ble_hal_stage_erase_block(i);
        if (RS_STATUS_SUCCESS != st)
        {
            break;
        }
    }

    rs_ota_fw_update_over_ble_hal_log("[FWACQ] open: erase 0x%08X (%u KB) -> st=%d\r\n",
                                      (unsigned) STAGE_ADDR, (unsigned) (STAGE_SIZE / 1024U),
                                      (int) st);
    return st;
}

/******************************************************************************
 * Append a received chunk, programming full pages as they fill.
 ******************************************************************************/
rs_status_t rs_ota_fw_update_over_ble_hal_stage_write(const uint8_t * p_data, uint32_t len)
{
    if ((NULL == p_data) || (0U == len))
    {
        return RS_STATUS_SUCCESS;
    }

    while (len > 0U)
    {
        if ((s_offset + s_page_fill) >= STAGE_SIZE)
        {
            rs_ota_fw_update_over_ble_hal_logs(RS_STATUS_ERR_OVERFLOW,
                                               "[FWACQ] overflow, dropping %u bytes\r\n",
                                               (unsigned) len);
            return RS_STATUS_ERR_OVERFLOW;
        }

        uint32_t space = PROGRAM_UNIT - s_page_fill;
        uint32_t n     = (len < space) ? len : space;

        memcpy(&s_page[s_page_fill], p_data, n);
        s_page_fill += n;
        p_data      += n;
        len         -= n;

        if (PROGRAM_UNIT == s_page_fill)
        {
            if (FSP_SUCCESS != rs_ota_fw_update_over_ble_hal_program_page())
            {
                return RS_STATUS_ERR_HARDWARE;
            }
        }
    }
    return RS_STATUS_SUCCESS;
}

/******************************************************************************
 * Flush a partial final page (padded with 0xFF) and report + read back.
 ******************************************************************************/
rs_status_t rs_ota_fw_update_over_ble_hal_stage_end(void)
{
    fsp_err_t err = FSP_SUCCESS;

    if (s_page_fill > 0U)
    {
        memset(&s_page[s_page_fill], 0xFF, PROGRAM_UNIT - s_page_fill);
        err = rs_ota_fw_update_over_ble_hal_program_page();
    }

    rs_ota_fw_update_over_ble_hal_log("[FWACQ] end: %u bytes stored @0x%08X (err=%d)\r\n",
                                      (unsigned) s_offset, (unsigned) STAGE_ADDR, (int) err);

    /* Quick read-back of the first bytes (code flash is memory-mapped). If this
     * looks stale, wrap the P/E ops with R_BSP_FlashCacheDisable()/Enable().
     * Built as ONE line: every call costs a trip through the application
     * callback, so a byte-per-call loop would spam the sink 16 times. */
    const uint8_t * p     = (const uint8_t *) STAGE_ADDR;
    const uint32_t  shown = (s_offset < 16U) ? s_offset : 16U;
    char            line[80];
    int             used  = snprintf(line, sizeof(line), "[FWACQ] readback:");

    for (uint32_t i = 0U; (i < shown) && (used > 0) && (((size_t) used) < sizeof(line)); i++)
    {
        int n = snprintf(&line[used], sizeof(line) - (size_t) used, " %02X", p[i]);
        if (n <= 0)
        {
            break;
        }
        used += n;
    }

    rs_ota_fw_update_over_ble_hal_log("%s\r\n", line);

    return (FSP_SUCCESS == err) ? RS_STATUS_SUCCESS : RS_STATUS_ERR_HARDWARE;
}

uint32_t rs_ota_fw_update_over_ble_hal_stage_written(void)
{
    return s_offset;
}

bool rs_ota_fw_update_over_ble_hal_stage_has_pending(void)
{
    return (s_page_fill > 0U);
}

rs_status_t rs_ota_fw_update_over_ble_hal_stage_read(uint32_t offset, void * p_dst, uint32_t len)
{
    if (NULL == p_dst)
    {
        return RS_STATUS_ERR_ASSERTION;
    }
    if ((offset > STAGE_SIZE) || (len > (STAGE_SIZE - offset)))
    {
        return RS_STATUS_ERR_OVERFLOW;
    }

    /* Code flash is memory-mapped, so the staged image can be read directly. */
    memcpy(p_dst, (const void *) (STAGE_ADDR + offset), len);
    return RS_STATUS_SUCCESS;
}

/* ---- transport (DA14531 BLE / SUOTA) --------------------------------------- *
 * Bring-up lives in the QE-generated BLE demo (ble/app_main.c): it opens the
 * GTL/BLE link, registers the SUOTA server and then runs the BLE event loop
 * forever. To keep the public API non-blocking, that loop gets its OWN task here
 * instead of running on the caller's thread. The loop also drives the staged
 * window erase and forwards transfer events back through the feed_* ingress
 * functions, which this layer relays to the orchestrator's callbacks. Keeping the
 * app_main reference in THIS file preserves the rule that only the HAL
 * translation unit knows about the BLE stack.
 *
 * The task is created statically (§12 rule 5: no dynamic allocation - and the
 * FreeRTOS heap is only 2 KB). Stack size and priority mirror the FSP-generated
 * BLE thread, which used to run this same loop.
 *
 * SHUTDOWN is cooperative: ..._transport_close() raises s_stop_requested, the
 * loop in app_main() breaks out at the end of its current pass, calls
 * ..._transport_shutdown() (disconnect, stop advertising) and then closes the
 * BLE module (RM_BLE_ABS_Close, generated code). The task then releases what
 * app_main() created, signals s_stop_done and parks; the closer deletes it. A
 * task is never deleted while it is inside the BLE stack unless it fails to stop
 * within RS_OTA_BLE_STOP_TIMEOUT_MS. */
extern void app_main(void);
extern EventGroupHandle_t g_ble_event_group_handle;   /* app_main.c (generated) */

#define RS_OTA_BLE_TASK_STACK_WORDS  (2048U / sizeof(StackType_t))
#define RS_OTA_BLE_TASK_PRIORITY     (2U)

/* Upper bound for a clean stop: one loop pass (woken at least every 250 ms, an
 * erase block can add ~240 ms) + disconnect + advertising stop. */
#define RS_OTA_BLE_STOP_TIMEOUT_MS   (5000U)

static StaticTask_t s_ble_task_tcb;
static StackType_t  s_ble_task_stack[RS_OTA_BLE_TASK_STACK_WORDS]
    BSP_ALIGN_VARIABLE(BSP_STACK_ALIGNMENT);
static TaskHandle_t s_ble_task_handle;

static volatile bool     s_stop_requested;
static StaticSemaphore_t s_stop_done_buf;
static SemaphoreHandle_t s_stop_done;

bool rs_ota_fw_update_over_ble_hal_transport_stop_requested(void)
{
    return s_stop_requested;
}

static void rs_ota_fw_update_over_ble_hal_ble_task(void * pvParameters)
{
    FSP_PARAMETER_NOT_USED(pvParameters);

    app_main();   /* returns only once a stop was requested and BLE is closed */

    s_transport_open = false;

    /* app_main() creates its event group on every run; free it so a Stop()/Start()
     * cycle does not leak the (2 KB) FreeRTOS heap. The UART is closed by now, so
     * nothing can signal it any more. */
    if (NULL != g_ble_event_group_handle)
    {
        EventGroupHandle_t eg = g_ble_event_group_handle;
        g_ble_event_group_handle = NULL;
        vEventGroupDelete(eg);
    }

    /* Park instead of self-deleting: a self-deleted task keeps its (static) TCB on
     * the idle task's termination list for a while, and the next Start() would
     * re-create a task in that same memory. Deleted by the closer instead, the
     * TCB is released immediately. */
    (void) xSemaphoreGive(s_stop_done);
    vTaskSuspend(NULL);
}

rs_status_t rs_ota_fw_update_over_ble_hal_transport_open(
    const rs_ota_fw_update_over_ble_hal_callbacks_t * p_cb)
{
    if (NULL == p_cb)
    {
        return RS_STATUS_ERR_ASSERTION;
    }

    if (NULL != s_ble_task_handle)
    {
        return RS_STATUS_SUCCESS;   /* already up; nothing to do */
    }

    if (NULL == s_stop_done)
    {
        s_stop_done = xSemaphoreCreateBinaryStatic(&s_stop_done_buf);
    }

    /* Register callbacks BEFORE the task can raise its first event. */
    s_cb             = *p_cb;
    s_stop_requested = false;
    s_transport_open = true;

    s_ble_task_handle = xTaskCreateStatic(rs_ota_fw_update_over_ble_hal_ble_task,
                                          "ota_ble",
                                          RS_OTA_BLE_TASK_STACK_WORDS,
                                          NULL,
                                          RS_OTA_BLE_TASK_PRIORITY,
                                          s_ble_task_stack,
                                          &s_ble_task_tcb);
    if (NULL == s_ble_task_handle)
    {
        s_transport_open = false;
        rs_ota_fw_update_over_ble_hal_logs(RS_STATUS_ERR_HARDWARE,
                                           "[FWACQ] BLE task create failed\r\n");
        return RS_STATUS_ERR_HARDWARE;
    }

    return RS_STATUS_SUCCESS;
}

rs_status_t rs_ota_fw_update_over_ble_hal_transport_close(void)
{
    TaskHandle_t handle = s_ble_task_handle;

    if (NULL == handle)
    {
        s_transport_open = false;
        return RS_STATUS_SUCCESS;   /* already closed */
    }

    /* The BLE task cannot wait for its own exit (e.g. Stop() called from the RS
     * callback, which runs on that task). Refuse rather than deadlock. */
    if (xTaskGetCurrentTaskHandle() == handle)
    {
        return RS_STATUS_ERR_ASSERTION;
    }

    s_transport_open = false;   /* stop relaying events first */

    (void) xSemaphoreTake(s_stop_done, 0);   /* drop a stale give, if any */
    s_stop_requested = true;

    /* The loop notices the request on its next pass (its 250 ms timer wakes it
     * even when the link is idle), shuts BLE down and signals s_stop_done. */
    bool clean = (pdTRUE == xSemaphoreTake(s_stop_done, pdMS_TO_TICKS(RS_OTA_BLE_STOP_TIMEOUT_MS)));

    /* Either parked (clean) or stuck: deleting it from this task releases the
     * static TCB immediately, so a later Start() can reuse it. */
    s_ble_task_handle = NULL;
    vTaskDelete(handle);
    s_stop_requested = false;

    if (!clean)
    {
        /* Last resort: the task is gone, so nothing else is using the stack -
         * close the module from here so the next Start() can re-open it. */
        (void) RM_BLE_ABS_Close(g_ble_abs0.p_ctrl);
        rs_ota_fw_update_over_ble_hal_logs(RS_STATUS_ERR_HARDWARE,
                                           "[BLE] transport did not stop within %u ms - task forced down\r\n",
                                           (unsigned) RS_OTA_BLE_STOP_TIMEOUT_MS);
        return RS_STATUS_ERR_HARDWARE;
    }

    return RS_STATUS_SUCCESS;
}

/* ---- state protection ------------------------------------------------------ *
 * Transfer events run on the BLE task while the application polls the public API
 * from its own thread, so the orchestrator's state needs guarding. The sections
 * are a handful of instructions (copy a few words), so an RTOS critical section
 * is the cheapest correct option. */
void rs_ota_fw_update_over_ble_hal_lock(void)
{
    taskENTER_CRITICAL();
}

void rs_ota_fw_update_over_ble_hal_unlock(void)
{
    taskEXIT_CRITICAL();
}

/* ---- link supervision state (see the block comment above ..._link_poll) ---- */
extern uint16_t                               g_conn_hdl;                  /* app_main.c (generated) */
extern ble_abs_legacy_advertising_parameter_t g_ble_advertising_parameter; /* app_main.c (generated) */
extern ble_status_t                           ble_init(void);              /* app_main.c (generated) */

#define LINK_MAX_BLOCK_LEN   (1024U)   /* larger bursts overrun the 115200 baud GTL link */
#define LINK_STALL_MS        (20000U)  /* the phone app gives up after 30 s               */
#define LINK_DISC_GUARD_MS   (5000U)
#define LINK_ADV_CHECK_MS    (1000U)
#define LINK_ADV_MAX_MISSES  (10U)

static volatile bool s_xfer_active;     /* feed_start .. feed_end / abort          */
static volatile bool s_xfer_rejected;   /* unsupported PATCH_LEN: drop all data    */
static volatile bool s_stage_invalid;   /* erase staging block 0 on the next poll  */
static volatile uint32_t s_xfer_rx_tick;
static bool          s_link_was_up;
static uint32_t      s_disc_tick;       /* 0 = no local disconnect pending         */
static uint32_t      s_adv_tick;
static uint32_t      s_adv_misses;
static volatile bool s_adv_on;          /* ADV_ON seen since the last connect/restart */

void rs_ota_fw_update_over_ble_hal_link_adv_on(void)
{
    s_adv_on     = true;
    s_adv_misses = 0U;
}

static bool link_connected(void)
{
    return BLE_GAP_INVALID_CONN_HDL != g_conn_hdl;
}

/* End an unfinished transfer: the RS leaves IN_PROGRESS and the half-written
 * image is invalidated (its first sector carries a valid vector table, so the slot
 * would otherwise read "image present" and S1 could install it). */
static void link_abort(const char * p_why)
{
    bool was_active = s_xfer_active;

    s_xfer_active   = false;
    s_xfer_rejected = false;

    if (was_active)
    {
        rs_ota_fw_update_over_ble_hal_logs(RS_STATUS_ERR_HARDWARE, "[SUOTA] transfer ABORTED (%s)\r\n", p_why);
        s_stage_invalid = true;

        if (s_transport_open && (NULL != s_cb.p_on_error))
        {
            s_cb.p_on_error(RS_STATUS_ERR_HARDWARE);
        }
    }
}

/* Rebuild the RA6E2 <-> DA14531 link WITHOUT resetting the MCU. R_BLE_Open()
 * (inside ble_init) hard-resets the DA14531 through its reset pin, restarts the
 * GTL parser and waits for DEVICE_READY; advertising then comes back through the
 * normal STACK_ON -> GET_ADDR path, exactly as at boot. Only if even that fails is
 * the radio unusable, and an MCU reset the last way back. */
static void link_restart(const char * p_why)
{
    rs_ota_fw_update_over_ble_hal_logs(RS_STATUS_ERR_HARDWARE,
                                       "[BLE] DA14531 link lost (%s) - restarting BLE stack\r\n", p_why);
    link_abort("BLE restart");

    (void) RM_BLE_ABS_Close(g_ble_abs0.p_ctrl);
    g_conn_hdl    = BLE_GAP_INVALID_CONN_HDL;
    s_adv_on      = false;
    s_link_was_up = false;
    s_disc_tick   = 0U;
    s_adv_misses  = 0U;
    s_adv_tick    = (uint32_t) xTaskGetTickCount();

    ble_status_t st = ble_init();

    if (BLE_SUCCESS != st)
    {
        rs_ota_fw_update_over_ble_hal_logs(RS_STATUS_ERR_HARDWARE,
                                           "[BLE] BLE stack restart FAILED (0x%04X) - resetting MCU\r\n",
                                           (unsigned) st);
        vTaskDelay(pdMS_TO_TICKS(200));   /* let the log line drain */
        NVIC_SystemReset();
    }
}

bool rs_ota_fw_update_over_ble_hal_link_block_len_ok(uint16_t block_len)
{
    if ((0U != block_len) && (block_len <= LINK_MAX_BLOCK_LEN))
    {
        return true;
    }

    /* The transfer stays "active" on purpose: if the client ignores the error and
     * neither stops nor disconnects, the stall rule still recovers the link. */
    s_xfer_rejected = true;
    rs_ota_fw_update_over_ble_hal_logs(RS_STATUS_ERR_OVERFLOW,
                                       "[SUOTA] Block length %u REJECTED (max %u)\r\n",
                                       (unsigned) block_len, (unsigned) LINK_MAX_BLOCK_LEN);
    return false;
}

/******************************************************************************
 * Link supervisor - the board must ALWAYS come back to advertising.
 *
 *  1. Link down with a transfer unfinished -> abort it, invalidate the staging.
 *  2. Disconnected and no ADV_ON since -> restart advertising once per
 *     LINK_ADV_CHECK_MS. (StartLegacyAdvertising() is NOT a usable probe: on the
 *     GTL port it succeeds - and restarts advertising - every time.)
 *  3. Connected, transfer stalled LINK_STALL_MS -> disconnect from our side.
 *  4. Disconnect refused, never confirmed, or advertising unconfirmed for
 *     LINK_ADV_MAX_MISSES checks -> the DA14531 link is lost: link_restart().
 ******************************************************************************/
void rs_ota_fw_update_over_ble_hal_link_poll(void)
{
    uint32_t now = (uint32_t) xTaskGetTickCount();

    /* A flash guard lost UART bytes: the GTL parser is out of step and will not
     * recover by itself. Rebuild the link now rather than wait for a stall. */
    if (s_link_resync)
    {
        s_link_resync = false;
        link_restart("UART bytes lost during flash operation");
        return;
    }

    if (s_stage_invalid)
    {
        s_stage_invalid = false;
        rs_status_t st = rs_ota_fw_update_over_ble_hal_stage_erase_block(0U);
        rs_ota_fw_update_over_ble_hal_log("[SUOTA] aborted transfer - staging invalidated (st=%d)\r\n", (int) st);

        /* Until block 0 is erased the half-written image still looks installable
         * (valid vector table), so keep retrying on the next passes. */
        if (RS_STATUS_SUCCESS != st)
        {
            s_stage_invalid = true;
        }
    }

    /* Not up yet: the BD address arrives with GET_ADDR_COMP, just before the boot
     * advertising starts. Before that there is nothing to supervise. */
    if (0U == (g_ble_advertising_parameter.own_bluetooth_address[0] |
               g_ble_advertising_parameter.own_bluetooth_address[1] |
               g_ble_advertising_parameter.own_bluetooth_address[5]))
    {
        return;
    }

    if (link_connected())
    {
        s_link_was_up = true;
        s_adv_on      = false;   /* the DA stops advertising on connect */
        s_adv_misses  = 0U;

        if (s_xfer_active && (0U == s_disc_tick) && ((now - s_xfer_rx_tick) > pdMS_TO_TICKS(LINK_STALL_MS)))
        {
            ble_status_t dst = R_BLE_GAP_Disconnect(g_conn_hdl, 0x13U /* remote user terminated */);
            rs_ota_fw_update_over_ble_hal_logs(RS_STATUS_ERR_HARDWARE,
                                               "[SUOTA] transfer stalled - disconnecting (0x%04X)\r\n",
                                               (unsigned) dst);
            link_abort("stalled");

            if (BLE_SUCCESS != dst)
            {
                link_restart("disconnect refused");
                return;
            }

            s_disc_tick = (0U == now) ? 1U : now;
        }

        if ((0U != s_disc_tick) && ((now - s_disc_tick) > pdMS_TO_TICKS(LINK_DISC_GUARD_MS)))
        {
            link_restart("no DISCONN_IND");
        }

        return;
    }

    s_disc_tick = 0U;

    if (s_link_was_up)
    {
        s_link_was_up = false;
        rs_ota_fw_update_over_ble_hal_log("[BLE] Disconnected\r\n");
        link_abort("link lost");
        s_adv_tick = now;   /* the generated DISCONN handler restarts advertising itself */
        return;
    }

    if (s_adv_on || ((now - s_adv_tick) <= pdMS_TO_TICKS(LINK_ADV_CHECK_MS)))
    {
        return;
    }

    s_adv_tick = now;

    fsp_err_t err = RM_BLE_ABS_StartLegacyAdvertising(g_ble_abs0.p_ctrl, &g_ble_advertising_parameter);

    s_adv_misses++;
    rs_ota_fw_update_over_ble_hal_logs(RS_STATUS_ERR_HARDWARE,
                                       "[BLE] not advertising - restart (err=%d, %u/%u)\r\n",
                                       (int) err, (unsigned) s_adv_misses, (unsigned) LINK_ADV_MAX_MISSES);

    if (s_adv_misses >= LINK_ADV_MAX_MISSES)
    {
        link_restart("advertising not confirmed");
    }
}

/* Legacy advertising set used by rm_ble_abs (BLE_ABS_LEGACY_HDL, private to
 * rm_ble_abs.c). */
#define LINK_LEGACY_ADV_HDL        (0x00U)
#define LINK_SHUTDOWN_DISC_MS      (1000U)
#define LINK_SHUTDOWN_ADV_MS       (500U)
#define LINK_SHUTDOWN_PUMP_MS      (10U)

/* Service the stack for @p ms, or until @p p_done returns true. */
static void link_pump(uint32_t ms, bool (*p_done)(void))
{
    uint32_t start = (uint32_t) xTaskGetTickCount();

    while (((uint32_t) xTaskGetTickCount() - start) < pdMS_TO_TICKS(ms))
    {
        R_BLE_Execute();
        if ((NULL != p_done) && p_done())
        {
            return;
        }
        vTaskDelay(pdMS_TO_TICKS(LINK_SHUTDOWN_PUMP_MS));
    }
}

static bool link_disconnected(void)
{
    return !link_connected();
}

static bool link_adv_confirmed(void)
{
    return s_adv_on;
}

/******************************************************************************
 * Leave the air cleanly before the BLE module is closed (called by app_main()
 * once ..._transport_stop_requested() made its loop exit; runs on the BLE task).
 *
 *  1. An unfinished transfer is aborted and its half-written image invalidated,
 *     exactly as on a lost link (no RS callback: the transport is already closed).
 *  2. A connected peer is disconnected, and the DISCONN_IND is waited for.
 *  3. Advertising is stopped. The generated DISCONN_IND handler restarts it, so
 *     wait for that ADV_ON first - stopping mid-restart would be undone by the
 *     ABS state machine finishing the start sequence.
 ******************************************************************************/
void rs_ota_fw_update_over_ble_hal_transport_shutdown(void)
{
    link_abort("transport stopped");
    if (s_stage_invalid)
    {
        rs_status_t est = rs_ota_fw_update_over_ble_hal_stage_erase_block(0U);

        /* Keep the flag on failure: the half-written image still looks
         * installable, and link_poll() retries after the next Start(). */
        s_stage_invalid = (RS_STATUS_SUCCESS != est);
        if (s_stage_invalid)
        {
            rs_ota_fw_update_over_ble_hal_logs(RS_STATUS_ERR_HARDWARE,
                                               "[SUOTA] stop: staging invalidate FAILED (st=%d) - retried after next Start()\r\n",
                                               (int) est);
        }
    }

    if (link_connected())
    {
        ble_status_t dst = R_BLE_GAP_Disconnect(g_conn_hdl, 0x13U /* remote user terminated */);
        if (BLE_SUCCESS == dst)
        {
            link_pump(LINK_SHUTDOWN_DISC_MS, link_disconnected);
        }

        if (link_connected())
        {
            rs_ota_fw_update_over_ble_hal_logs(RS_STATUS_ERR_HARDWARE,
                                               "[BLE] stop: disconnect not confirmed (0x%04X)\r\n",
                                               (unsigned) dst);
        }
    }

    if (!link_connected())
    {
        link_pump(LINK_SHUTDOWN_ADV_MS, link_adv_confirmed);
    }

    ble_status_t ast = R_BLE_GAP_StopAdv(LINK_LEGACY_ADV_HDL);
    link_pump(LINK_SHUTDOWN_PUMP_MS * 10U, NULL);   /* let the command complete */

    rs_ota_fw_update_over_ble_hal_log("[BLE] stop: advertising stopped (0x%04X) - closing BLE\r\n",
                                      (unsigned) ast);

    /* Fresh supervisor state for the next Start(). */
    g_conn_hdl    = BLE_GAP_INVALID_CONN_HDL;
    s_adv_on      = false;
    s_link_was_up = false;
    s_disc_tick   = 0U;
    s_adv_misses  = 0U;
}

void rs_ota_fw_update_over_ble_hal_feed_start(void)
{
    /* A client that vanished during the staging erase has nothing to start. */
    if (!link_connected())
    {
        return;
    }

    s_xfer_active   = true;
    s_xfer_rejected = false;
    s_stage_invalid = false;
    s_xfer_rx_tick  = (uint32_t) xTaskGetTickCount();

    if (s_transport_open && (NULL != s_cb.p_on_start))
    {
        s_cb.p_on_start();
    }
}

void rs_ota_fw_update_over_ble_hal_feed_block(const uint8_t * p_data, uint32_t len)
{
    /* Outside a transfer, or after a rejected block size: never stage it. */
    if (!s_xfer_active || s_xfer_rejected)
    {
        return;
    }

    s_xfer_rx_tick = (uint32_t) xTaskGetTickCount();

    if (s_transport_open && (NULL != s_cb.p_on_block))
    {
        s_cb.p_on_block(p_data, len);
    }
}

void rs_ota_fw_update_over_ble_hal_feed_end(void)
{
    s_xfer_active = false;

    if (s_transport_open && (NULL != s_cb.p_on_end))
    {
        s_cb.p_on_end();
    }
}

void rs_ota_fw_update_over_ble_hal_feed_install_request(void)
{
    if (s_transport_open && (NULL != s_cb.p_on_install_request))
    {
        s_cb.p_on_install_request();
    }
}

void rs_ota_fw_update_over_ble_hal_feed_error(rs_status_t status)
{
    FSP_PARAMETER_NOT_USED(status);

    /* Same teardown as every other failed transfer (and the RS error relay). */
    link_abort("transport error / client ABORT");
}
