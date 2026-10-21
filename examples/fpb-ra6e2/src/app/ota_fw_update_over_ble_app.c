/******************************************************************************
 * File Name    : ota_fw_update_over_ble_app.c
 * Description  : Reference/demo application entry for the
 *                ota_fw_update_over_ble RS.
 *
 *                The FSP-generated BLE thread entry (src/ble_thread_entry.c)
 *                delegates to app_ota_fw_update_over_ble_entry() here, so
 *                the authored demo logic lives in src/app/, outside the
 *                files FSP/QE regenerate.
 *
 *                The application talks to the RS through its PUBLIC API only
 *                (rs_ota_fw_update_over_ble.h): no HAL calls, no direct call
 *                into the generated BLE demo.
 ******************************************************************************/
#include "ota_fw_update_over_ble_app.h"
#include "rs_ota_fw_update_over_ble.h"
#include "ota_fw_update_over_ble_log.h"

#include "bsp_api.h"
#include "bsp_pin_cfg.h"
#include "FreeRTOS.h"
#include "task.h"

/** Poll period of the status loop below. */
#define APP_POLL_PERIOD_MS          (250U)

/** Manual install trigger: the FPB-RA6E2 user button S1.
 *
 *  S1 is on P304, already configured as an input by the generated pin data
 *  (bsp_pin_cfg.h exports it as SW1), so this costs no new FSP module: the pin
 *  is READ from the poll loop below rather than driven through an ICU/external
 *  IRQ instance. That is deliberate - an ISR here would buy nothing (the work
 *  it would trigger has to happen on this task anyway) and would need a
 *  configuration change plus a regeneration to exist at all.
 *
 *  The board ties S1 to GND through the switch, so a press reads LOW. */
#define APP_SWAP_BUTTON_PIN         (SW1)
#define APP_SWAP_BUTTON_ACTIVE      (BSP_IO_LEVEL_LOW)

/** Swap mode used by the button trigger.
 *
 *  Kept the same as the automatic path: installing on TRIAL means a manual
 *  install of a broken image is still undone by the next reset. Change this to
 *  ..._SWAP_MODE_AFTER_RESET for a permanent install with no safety net. */
#define APP_BUTTON_SWAP_MODE        (RS_OTA_FW_UPDATE_OVER_BLE_SWAP_MODE_AFTER_RESET_WITH_TEST)

/** How long a freshly installed TRIAL image must keep running before it declares
 *  itself healthy and confirms.
 *
 *  This is the application's own judgement call, not something the RS or the
 *  bootloader can make for it. The gate used here is deliberately simple and
 *  states exactly what it proves: the image booted, BLE bring-up succeeded and
 *  the scheduler kept this task alive for the period below. An application with
 *  a better definition of "healthy" (sensors responding, a server reachable,
 *  a self-test passing) should replace this with that definition.
 *
 *  Confirming too eagerly is the classic mistake: a trial that always confirms
 *  is a permanent swap with extra steps. */
#define APP_TRIAL_HEALTH_GATE_MS    (5000U)

/** Log delay before a self-inflicted reset, so RTT output is not truncated. */
#define APP_PRE_RESET_MS            (200U)

/** Pause between the "install armed" block and the "resetting now" line. */
#define APP_INSTALL_NOTICE_MS       (1000U)

/** Progress line period while a trial image runs its health gate. */
#define APP_TRIAL_PROGRESS_MS       (1000U)

/** True while this boot is a trial waiting for its health gate. The RS's own
 *  lines ("[OTA] ready - waiting for a SUOTA client ...") are muted meanwhile:
 *  the gate ends in a confirm-reset within seconds, so offering a download here
 *  would be misleading - it would be cut off by that reset. */
static volatile bool s_trial_quiet = false;


/** The RS's single callback: workflow events AND diagnostic lines all arrive
 *  here. Runs on the RS transport's BLE task, so it must stay non-blocking
 *  (OTA_LOG is). We just emit the ready-made summary line.
 *
 *  Lines the RS produced before this was registered are replayed by the RS on
 *  registration, so start-up diagnostics are not lost. */
static void ota_cb(rs_status_t status, const char * summary, void * p_context)
{
    (void)status;
    (void)p_context;

    if (s_trial_quiet)
    {
        return;
    }

    OTA_LOG("%s", summary);
}

/** Current state of the user button. */
static bool app_swap_button_is_pressed(void)
{
    return (APP_SWAP_BUTTON_ACTIVE == (bsp_io_level_t) R_BSP_PinRead(APP_SWAP_BUTTON_PIN));
}

/** Arms the staged image for install and resets into the bootloader.
 *
 *  Shared by the automatic path (download completed) and the manual one (user
 *  button), so both reach the bootloader the same way - the only difference
 *  being what decided to go.
 *
 *  Returns only if the install was REFUSED - by the pre-checks below or by the
 *  seam (e.g. no bootloader underneath this build). In that case nothing has
 *  changed and the caller carries on.
 *
 *  Log style: one line per decision, "<source>: <what happens>". */
static const char * app_swap_mode_name(rs_ota_fw_update_over_ble_swap_mode_t mode)
{
    return (RS_OTA_FW_UPDATE_OVER_BLE_SWAP_MODE_AFTER_RESET_WITH_TEST == mode) ? "TRIAL" : "PERMANENT";
}

static const char * app_refusal_reason(rs_status_t st)
{
    switch (st)
    {
        case RS_STATUS_ERR_UNSUPPORTED: return "no bootloader support in this build";
        case RS_STATUS_ERR_ASSERTION:   return "invalid request";
        default:                        return "bootloader seam error";
    }
}

static void app_install_staged_image(rs_ota_fw_update_over_ble_swap_mode_t mode,
                                     const char *                          p_source,
                                     rs_ota_fw_update_over_ble_status_t    transfer)
{
    /* A transfer is writing the slot right now: installing a half-written image
     * is pointless, and reading the slot to check it would collide with the
     * flash programming. */
    if (RS_OTA_FW_UPDATE_OVER_BLE_STATUS_IN_PROGRESS == transfer)
    {
        OTA_LOG("[APP] %s: install refused - transfer in progress\r\n", p_source);
        return;
    }

    /* Nothing to install: the bootloader would reject it anyway, but only after a
     * reboot. Say so now instead. */
    rs_ota_fw_update_over_ble_slot_info_t slot = {0};

    if ((RS_STATUS_SUCCESS == rs_ota_fw_update_over_ble_SecondarySlotGet(&slot)) && (!slot.image_present))
    {
        OTA_LOG("[APP] %s: install refused - secondary slot is empty\r\n", p_source);
        return;
    }

    rs_status_t st = rs_ota_fw_update_over_ble_Swap(mode);

    if (RS_STATUS_SUCCESS != st)
    {
        OTA_LOG("[APP] %s: install refused - %s (st=%d)\r\n", p_source, app_refusal_reason(st), (int) st);
        return;
    }

    /* Arming only RECORDS the request; the reset is ours to choose. Drop the
     * BLE link first so the peer sees a clean disconnect instead of a device
     * that vanishes mid-session. */
    (void) rs_ota_fw_update_over_ble_Stop();

    OTA_LOG("[APP] ================================================================\r\n");
    OTA_LOG("[APP]  INSTALL      requested by %s - %s image armed\r\n", p_source, app_swap_mode_name(mode));

    /* WHAT is being installed. The slot is still readable: no transfer is
     * running (checked above) and the swap only happens after the reset. */
    ota_log_slot_image("image        ");

    if (RS_OTA_FW_UPDATE_OVER_BLE_SWAP_MODE_AFTER_RESET_WITH_TEST == mode)
    {
        OTA_LOG("[APP]  safety net   must confirm within %u s of boot, else rolls back\r\n",
                (unsigned int) (APP_TRIAL_HEALTH_GATE_MS / 1000U));
    }
    else
    {
        OTA_LOG("[APP]  safety net   NONE - permanent install, no rollback\r\n");
    }

    OTA_LOG("[APP]  BLE          link closed\r\n");
    OTA_LOG("[APP] ================================================================\r\n");

    /* A deliberate beat between "armed" and "gone", so the operator sees the
     * decision before the reset lands. */
    vTaskDelay(pdMS_TO_TICKS(APP_INSTALL_NOTICE_MS));

    OTA_LOG("[APP] >>> RESETTING NOW - bootloader takes over <<<\r\n");

    /* Give RTT time to drain the line above before the core goes away. */
    vTaskDelay(pdMS_TO_TICKS(APP_PRE_RESET_MS));
    NVIC_SystemReset();
}

void app_ota_fw_update_over_ble_entry(void)
{
    /* Application trace output goes through OTA_LOG() (ota_fw_update_over_ble_log.h),
     * backed by SEGGER RTT channel 0 in non-blocking (skip) mode. It MUST stay
     * non-blocking: a blocking write from the BLE thread stalls R_BLE_Execute(),
     * the DA14531 GTL/UART link stops being serviced, and the phone SUOTA app
     * times out (~20 s) and disconnects.
     *
     * RTT rides the SWD/J-Link connection, NOT a UART. SCI0 is wired to the
     * DA14531 on PMOD2, so a serial console would collide with the BLE transport;
     * there is no CLI in this application for that reason. */
    ota_log_init();

    /* Ask the bootloader what it did on THIS boot, before anything else can
     * confuse the picture. The answer that matters is trial_pending: if the
     * running image was installed on trial and never confirms, the next reset
     * puts the previous image back. */
    rs_ota_fw_update_over_ble_boot_result_t boot = {0};
    (void) rs_ota_fw_update_over_ble_BootResultGet(&boot);

    /* Versions (app, bootloader, secondary slot), reset cause and boot outcome.
     * Must run BEFORE Start(): it reads the flash directly, which cannot overlap
     * a download.
     *
     * A trial boot gets its own compact block instead: the full banner already
     * ran on the boot that armed the install and runs again on the confirmed
     * boot right after this one, so repeating it here only adds noise.
     * REVERTED / BAD_IMAGE outcomes are spelled out on the banner's boot line. */
    if (boot.trial_pending)
    {
        s_trial_quiet = true;
        ota_log_banner_trial(APP_TRIAL_HEALTH_GATE_MS);
    }
    else
    {
        ota_log_banner(&boot);
    }

    const rs_ota_fw_update_over_ble_cfg_t cfg =
    {
        .version    = 1U,
        .size       = (uint16_t)sizeof(rs_ota_fw_update_over_ble_cfg_t),
        .mode       = RS_OTA_FW_UPDATE_OVER_BLE_EXECUTION_MODE_NON_BLOCKING,
        .stage_addr = 0U,            /* 0 => board preset */
        .stage_size = 0U,            /* 0 => board preset */
        .p_callback = ota_cb,
        .p_context  = NULL,
    };

    /* Single entry point: the RS validates its dependencies and brings up the
     * DA14531 BLE/SUOTA transport, which runs on its own task. This returns as
     * soon as the link is armed. The firmware staging window is erased by the RS
     * when a download actually starts, not here. */
    rs_status_t st = rs_ota_fw_update_over_ble_Start(&cfg, NULL);
    if (RS_STATUS_SUCCESS != st)
    {
        OTA_LOG("[APP] Start failed (st=%d)\r\n", (int)st);

        /* On a trial image this is the right failure mode: refusing to confirm
         * means the next reset restores the image that did work. */
        while (1)
        {
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
    }

    /* Non-blocking: this thread is free. Poll the workflow state and report the
     * final verdict once a download completes. */
    rs_ota_fw_update_over_ble_status_t last = RS_OTA_FW_UPDATE_OVER_BLE_STATUS_IDLE;

    /* Set once a completed download has been armed for install, so a repeated
     * reboot request cannot arm it twice. */
    bool     install_armed  = false;
    /* Set once the "image staged" line has been logged for this download, so the
     * poll loop does not repeat it every 250 ms while waiting for the request. */
    bool     staged_reported = false;
    uint32_t healthy_ms     = 0U;

    /* Seeded with the CURRENT button state, not "released": a board powered up
     * with S1 held must not read as a press on the first poll. */
    bool button_was_pressed = app_swap_button_is_pressed();

    if (!boot.trial_pending)
    {
        OTA_LOG("[APP] press S1 to install the image in the secondary slot \r\n");
    }

    while (1)
    {
        rs_ota_fw_update_over_ble_result_t res = {0};

        if (RS_STATUS_SUCCESS == rs_ota_fw_update_over_ble_StatusGet(&res))
        {
            if (res.status != last)
            {
                last = res.status;

                /* The RS has already announced the new state through ota_cb()
                 * ("[OTA] ..."), so only add what that line cannot say. */
                switch (res.status)
                {
                    case RS_OTA_FW_UPDATE_OVER_BLE_STATUS_IN_PROGRESS:
                        /* A fresh download replaces whatever was staged, so the
                         * "image staged" report is due again once it completes. */
                        staged_reported = false;
                        break;

                    case RS_OTA_FW_UPDATE_OVER_BLE_STATUS_BAD_HEADER:
                    case RS_OTA_FW_UPDATE_OVER_BLE_STATUS_BAD_CRC:
                    case RS_OTA_FW_UPDATE_OVER_BLE_STATUS_NO_MEMORY:
                    case RS_OTA_FW_UPDATE_OVER_BLE_STATUS_TIMEOUT:
                        /* The slot was erased when the transfer started, so the
                         * image that was there before is gone too. */
                        OTA_LOG("[APP] transfer FAILED after %u of %u bytes - secondary slot "
                                "no longer holds an installable image\r\n",
                                (unsigned int)res.bytes_received,
                                (unsigned int)res.code_size);
                        break;

                    default:
                        /* WAITING / SUCCESS / IDLE: the [OTA] line says it all. */
                        break;
                }
            }

            if ((RS_OTA_FW_UPDATE_OVER_BLE_STATUS_SUCCESS == res.status) &&
                (!staged_reported))
            {
                staged_reported = true;

                OTA_LOG("[APP] image staged + verified (%u bytes) - awaiting "
                        "SUOTA reboot request or S1\r\n",
                        (unsigned int)res.bytes_received);
            }

            /* Install ONLY when the peer has asked for it.
             *
             * A verified image is not a request to reboot: the SUOTA client ends
             * the transfer (MEM_DEV END) and then decides, in its own time,
             * whether to activate it (MEM_DEV REBOOT). Installing on status
             * alone resets the board while the phone is still talking to it, so
             * the transfer looks like it failed even though it worked.
             *
             * If the request never comes - the user cancels, or the link simply
             * drops - nothing happens: the image stays staged and this boot keeps
             * running. It can still be installed later by S1, or replaced by the
             * next download. */
            if (res.install_requested && (!install_armed))
            {
                install_armed = true;

                /* Install on trial rather than permanently: if the new image
                 * cannot boot, cannot bring BLE up, or hangs before its health
                 * gate, the bootloader puts this one back on the next reset.
                 * A permanent install has no such second chance.
                 *
                 * Returns only if arming was refused. */
                app_install_staged_image(
                        RS_OTA_FW_UPDATE_OVER_BLE_SWAP_MODE_AFTER_RESET_WITH_TEST,
                        "SUOTA", res.status);
            }
        }

        /* Manual trigger: user button S1.
         *
         * Acts on the PRESS edge only, so holding the button down cannot arm
         * the swap repeatedly, and a button already held at start-up is ignored
         * until it has been released. The 250 ms sample period is far longer
         * than any contact bounce, so no extra debounce is needed.
         *
         * This deliberately does not look at install_armed: the button is the
         * developer overriding the automatic policy, and its whole purpose is
         * to work on a boot where no download just happened - the image staged
         * by an EARLIER session is still sitting in the secondary slot. */
        const bool button_pressed = app_swap_button_is_pressed();

        if (button_pressed && (!button_was_pressed))
        {
            if (boot.trial_pending)
            {
                /* The secondary slot holds the image being replaced; installing
                 * it mid-trial would fight the rollback the bootloader already
                 * owns. The gate decides this boot. */
                OTA_LOG("[APP] S1: ignored - trial in progress, %u s to confirm\r\n",
                        (unsigned int) ((APP_TRIAL_HEALTH_GATE_MS - healthy_ms) / 1000U));
            }
            else
            {
                app_install_staged_image(APP_BUTTON_SWAP_MODE, "S1", last);
            }
        }

        button_was_pressed = button_pressed;

        /* Health gate for a trial image. Only runs when this boot is actually on
         * trial; on an ordinary boot there is nothing to confirm.
         *
         * A trial image that got here through an ABNORMAL reset (watchdog, low
         * voltage, error detection) has already shown it is not healthy, so it is
         * never confirmed: the refusal is logged once and the next reset reverts
         * to the previous image. With the current bootloader any reset of an
         * unconfirmed trial image reverts it before it could run again, so this
         * is a defensive rule - it keeps the policy right if that ever changes. */
        if (boot.trial_pending && ota_log_reset_abnormal())
        {
            OTA_LOG("[APP] !! trial image NOT confirmed: last reset was %s - "
                    "the next reset reverts it\r\n", ota_log_reset_name());
            boot.trial_pending = false;
            s_trial_quiet      = false;
        }

        if (boot.trial_pending)
        {
            healthy_ms += APP_POLL_PERIOD_MS;

            if ((0U == (healthy_ms % APP_TRIAL_PROGRESS_MS)) && (healthy_ms < APP_TRIAL_HEALTH_GATE_MS))
            {
                OTA_LOG("[APP] trial: healthy %2u s / %u s\r\n",
                        (unsigned int) (healthy_ms / 1000U),
                        (unsigned int) (APP_TRIAL_HEALTH_GATE_MS / 1000U));
            }

            if (healthy_ms >= APP_TRIAL_HEALTH_GATE_MS)
            {
                OTA_LOG("[APP] trial: PASSED %u s - confirming image, resetting\r\n",
                        (unsigned int)(healthy_ms / 1000U));

                /* Confirm makes the running image permanent. It resets on the
                 * way through (the bootloader applies the confirmation out of
                 * reset), so this call does not return. */
                boot.trial_pending = false;

                vTaskDelay(pdMS_TO_TICKS(APP_PRE_RESET_MS));

                st = rs_ota_fw_update_over_ble_Swap(
                        RS_OTA_FW_UPDATE_OVER_BLE_SWAP_MODE_CONFIRM);

                /* Only reached if the seam refused, e.g. no bootloader in this
                 * build. Nothing was confirmed, so the next reset reverts. */
                s_trial_quiet = false;
                OTA_LOG("[APP] Swap(confirm) returned (st=%d)\r\n", (int)st);
            }
        }

        vTaskDelay(pdMS_TO_TICKS(APP_POLL_PERIOD_MS));
    }
}
