/******************************************************************************
 * File Name    : ota_fw_update_over_ble_log.c
 * Description  : SEGGER RTT back-end for OTA_LOG() / OTA_LOG_HEX(), the
 *                application version record, the reset-cause snapshot and the
 *                boot banner.
 *
 *                Up-buffer 0 is configured as SEGGER_RTT_MODE_NO_BLOCK_SKIP:
 *                the SUOTA path logs from the BLE thread, and a blocking write
 *                with no RTT viewer attached would stall R_BLE_Execute(), stop
 *                the DA14531 GTL/UART link being serviced, and make the phone
 *                SUOTA app time out (~20 s) and disconnect. Output is therefore
 *                dropped rather than blocking when the FIFO is full.
 ******************************************************************************/
#include "ota_fw_update_over_ble_log.h"
#include "bsp_api.h"
#include <stddef.h>
#include <string.h>

/*******************************************************************************
 * Version record - always compiled, so every image carries it
 *
 * Lives in ordinary .rodata and is found by SCANNING for its tag: a fixed
 * offset would pad every OTA .bin to the full slot size. The scanner's own code
 * contains the magic as a literal (and so does the image being scanned), so a
 * match also needs the complement word - never emitted as a literal - a matching
 * check word and two terminated strings.
 ******************************************************************************/
#define VER_MAGIC       (0x52565041UL)          /* 'A','P','V','R' little endian */
#define VER_LAYOUT      (1U)
#define VER_TEXT_LEN    (24U)
#define VER_CHECK(l, a, b, c)  (VER_MAGIC ^ 0xA5C3F00FUL ^ (((uint32_t) (l) << 24) | ((uint32_t) (a) << 16) | \
                                                           ((uint32_t) (b) << 8) | (uint32_t) (c)))
#define VER_NUM(p)      (((uint32_t) (p)->major << 16) | ((uint32_t) (p)->minor << 8) | (uint32_t) (p)->patch)

typedef struct
{
    uint32_t magic;
    uint32_t magic_inv;
    uint8_t  layout;
    uint8_t  major;
    uint8_t  minor;
    uint8_t  patch;
    char     name[VER_TEXT_LEN];
    char     build[VER_TEXT_LEN];                /* __DATE__ " " __TIME__ */
    uint32_t check;
} ver_t;                                         /* 64 bytes, layout VER_LAYOUT */

typedef char ver_name_fits[(sizeof(APP_VERSION_NAME) <= VER_TEXT_LEN) ? 1 : -1];
typedef char ver_size_ok[(64U == sizeof(ver_t)) ? 1 : -1];

__attribute__((used, aligned(4)))
static const ver_t s_ver =
{
    .magic     = VER_MAGIC,
    .magic_inv = ~VER_MAGIC,
    .layout    = (uint8_t) VER_LAYOUT,
    .major     = (uint8_t) APP_VERSION_MAJOR,
    .minor     = (uint8_t) APP_VERSION_MINOR,
    .patch     = (uint8_t) APP_VERSION_PATCH,
    .name      = APP_VERSION_NAME,
    .build     = __DATE__ " " __TIME__,
    .check     = VER_CHECK(VER_LAYOUT, APP_VERSION_MAJOR, APP_VERSION_MINOR, APP_VERSION_PATCH),
};

/*******************************************************************************
 * Reset cause - always compiled (the trial health gate needs it)
 *
 * RSTSR0, RSTSR1 and RSTSR2 are packed into one word: bits 0-7, 8-23, 24-31.
 * A RES-pin reset sets no flag: it shows as a WARM start (CWSF) with no cause.
 ******************************************************************************/
#define RST0(m)         ((uint32_t) (m))
#define RST1(m)         ((uint32_t) (m) << 8)
#define RST2(m)         ((uint32_t) (m) << 24)

#define RST_ABNORMAL    (RST0(R_SYSTEM_RSTSR0_LVD0RF_Msk | R_SYSTEM_RSTSR0_LVD1RF_Msk | R_SYSTEM_RSTSR0_LVD2RF_Msk) | \
                         RST1(R_SYSTEM_RSTSR1_IWDTRF_Msk | R_SYSTEM_RSTSR1_WDTRF_Msk  | R_SYSTEM_RSTSR1_RPERF_Msk  | \
                              R_SYSTEM_RSTSR1_REERF_Msk  | R_SYSTEM_RSTSR1_BUSSRF_Msk | R_SYSTEM_RSTSR1_BUSMRF_Msk | \
                              R_SYSTEM_RSTSR1_SPERF_Msk  | R_SYSTEM_RSTSR1_TZERF_Msk  | R_SYSTEM_RSTSR1_CPERF_Msk))

static uint32_t s_rst;

/* Most serious first: when several flags are set (an earlier boot never reached
 * the capture), the worst one is reported. A power-on reset clears the others. */
static const struct
{
    uint32_t     mask;
    const char * name;
} s_rst_names[] =
{
    { RST0(R_SYSTEM_RSTSR0_PORF_Msk),    "POWER-ON"             },
    { RST0(R_SYSTEM_RSTSR0_LVD0RF_Msk),  "LOW VOLTAGE (LVD0)"   },
    { RST0(R_SYSTEM_RSTSR0_LVD1RF_Msk),  "LOW VOLTAGE (LVD1)"   },
    { RST0(R_SYSTEM_RSTSR0_LVD2RF_Msk),  "LOW VOLTAGE (LVD2)"   },
    { RST1(R_SYSTEM_RSTSR1_IWDTRF_Msk),  "INDEPENDENT WATCHDOG" },
    { RST1(R_SYSTEM_RSTSR1_WDTRF_Msk),   "WATCHDOG"             },
    { RST1(R_SYSTEM_RSTSR1_RPERF_Msk),   "SRAM PARITY ERROR"    },
    { RST1(R_SYSTEM_RSTSR1_REERF_Msk),   "SRAM ECC ERROR"       },
    { RST1(R_SYSTEM_RSTSR1_BUSSRF_Msk),  "BUS SLAVE MPU ERROR"  },
    { RST1(R_SYSTEM_RSTSR1_BUSMRF_Msk),  "BUS MASTER MPU ERROR" },
    { RST1(R_SYSTEM_RSTSR1_SPERF_Msk),   "STACK POINTER ERROR"  },
    { RST1(R_SYSTEM_RSTSR1_TZERF_Msk),   "TRUSTZONE ERROR"      },
    { RST1(R_SYSTEM_RSTSR1_CPERF_Msk),   "CACHE PARITY ERROR"   },
    { RST0(R_SYSTEM_RSTSR0_DPSRSTF_Msk), "DEEP STANDBY EXIT"    },
    { RST1(R_SYSTEM_RSTSR1_SWRF_Msk),    "SOFTWARE"             },
};

/* Runs from __init_array, which the C runtime walks immediately after the
 * R_BSP_WarmStart(BSP_WARM_START_POST_C) hook and before bsp_init()/main()
 * (see ra/fsp/src/bsp/mcu/all/bsp_common.c). Nothing in between touches RSTSR,
 * so this is the first code to see the flags - and it needs no call site, which
 * keeps the generated hal_warmstart.c untouched. */
static __attribute__((constructor)) void reset_capture(void)
{
    s_rst = RST0(R_SYSTEM->RSTSR0) | RST1(R_SYSTEM->RSTSR1) | RST2(R_SYSTEM->RSTSR2);

    /* Flags clear by writing 0 after being read as 1, so this clears exactly
     * what was captured. CWSF can only be written to 1 and only a power-on
     * reset clears it: every later reset reads as WARM. */
    R_SYSTEM->RSTSR0 = 0U;
    R_SYSTEM->RSTSR1 = 0U;
    R_SYSTEM->RSTSR2 = (uint8_t) R_SYSTEM_RSTSR2_CWSF_Msk;
}

static bool reset_cold(void)
{
    return 0U == (s_rst & RST2(R_SYSTEM_RSTSR2_CWSF_Msk));
}

const char * ota_log_reset_name(void)
{
    for (uint32_t i = 0U; i < (sizeof(s_rst_names) / sizeof(s_rst_names[0])); i++)
    {
        if (0U != (s_rst & s_rst_names[i].mask))
        {
            return s_rst_names[i].name;
        }
    }

    return reset_cold() ? "POWER-ON (no flag)" : "RESET PIN";
}

bool ota_log_reset_abnormal(void)
{
    return 0U != (s_rst & RST_ABNORMAL);
}

#ifdef OTA_LOG_ENABLE

#include "SEGGER_RTT.h"
#include <stdarg.h>

#define OTA_LOG_RTT_CH    (0)
#define RULE              "[APP] ================================================================\r\n"

/*******************************************************************************
 * RTT back-end
 ******************************************************************************/
static bool s_initialised = false;

void ota_log_init(void)
{
    if (!s_initialised)
    {
        s_initialised = true;
        SEGGER_RTT_Init();

        /* Must never block: see file header. */
        (void) SEGGER_RTT_ConfigUpBuffer(OTA_LOG_RTT_CH, NULL, NULL, 0, SEGGER_RTT_MODE_NO_BLOCK_SKIP);
    }
}

void ota_log_printf(const char * p_format, ...)
{
    va_list args;

    ota_log_init();

    va_start(args, p_format);
    (void) SEGGER_RTT_vprintf(OTA_LOG_RTT_CH, p_format, &args);
    va_end(args);
}

void ota_log_hex(const char * p_label, const unsigned char * p_data, unsigned int len)
{
    ota_log_init();

    if (NULL != p_label)
    {
        (void) SEGGER_RTT_WriteString(OTA_LOG_RTT_CH, p_label);
    }

    for (unsigned int i = 0U; (NULL != p_data) && (i < len); i++)
    {
        (void) SEGGER_RTT_printf(OTA_LOG_RTT_CH, " %02X", (unsigned) p_data[i]);
    }

    (void) SEGGER_RTT_WriteString(OTA_LOG_RTT_CH, "\r\n");
}

/*******************************************************************************
 * Version scan / compare
 ******************************************************************************/

/** Search [base, base + size) of memory-mapped flash for a valid record. The
 *  record is 4-byte aligned, so only aligned words are looked at. */
static const ver_t * ver_find(uint32_t base, uint32_t size)
{
    for (uint32_t addr = (base + 3U) & ~3UL; (addr + sizeof(ver_t)) <= (base + size); addr += 4U)
    {
        const ver_t * p = (const ver_t *) addr;

        if ((VER_MAGIC == *(const volatile uint32_t *) addr) && (p->magic_inv == ~p->magic) &&
            (VER_LAYOUT == p->layout) && (p->check == VER_CHECK(p->layout, p->major, p->minor, p->patch)) &&
            (NULL != memchr(p->name, 0, VER_TEXT_LEN)) && (NULL != memchr(p->build, 0, VER_TEXT_LEN)))
        {
            return p;
        }
    }

    return NULL;
}

static const char * ver_compare(const ver_t * p, bool image_present)
{
    if (NULL == p)
    {
        return image_present ? "image without version record" : "empty";
    }

    if (VER_NUM(p) > VER_NUM(&s_ver))
    {
        return "NEWER - pending install";
    }

    if (VER_NUM(p) < VER_NUM(&s_ver))
    {
        return "older - rollback copy";
    }

    return (0 == strncmp(p->build, s_ver.build, VER_TEXT_LEN)) ? "same version, same build"
                                                                : "same version, DIFFERENT build";
}

/** "[APP]  <label>name vX.Y.Z  (build ...)" - no line end, the caller adds it. */
static void ver_print(const char * p_label, const ver_t * p)
{
    OTA_LOG("[APP]  %s%s v%u.%u.%u  (build %s)", p_label, p->name,
            (unsigned) p->major, (unsigned) p->minor, (unsigned) p->patch, p->build);
}

void ota_log_slot_image(const char * p_label)
{
    rs_ota_fw_update_over_ble_slot_info_t slot = {0};

    if (RS_STATUS_SUCCESS != rs_ota_fw_update_over_ble_SecondarySlotGet(&slot))
    {
        OTA_LOG("[APP]  %sn/a (no bootloader support in this build)\r\n", p_label);
        return;
    }

    /* Only scan a slot that holds an image: an erased or half-written slot has
     * nothing to report and is not worth 24 K flash reads. */
    const ver_t * p = slot.image_present ? ver_find(slot.base, slot.size) : NULL;

    if (NULL != p)
    {
        ver_print(p_label, p);
        OTA_LOG(" @0x%08X - %s\r\n", (unsigned) slot.base, ver_compare(p, true));
    }
    else
    {
        OTA_LOG("[APP]  %s@0x%08X - %s\r\n", p_label, (unsigned) slot.base, ver_compare(NULL, slot.image_present));
    }
}

/*******************************************************************************
 * Banners
 ******************************************************************************/

/* Indexed by rs_ota_fw_update_over_ble_boot_outcome_t. */
static const char * const s_outcome[] =
{
    "UNKNOWN (no report - cold start?)",
    "NORMAL",
    "SWAPPED",
    "REVERTED - previous update was rolled back",
    "BAD_IMAGE - staged image was rejected",
};

void ota_log_banner(const rs_ota_fw_update_over_ble_boot_result_t * p_boot)
{
    rs_ota_fw_update_over_ble_bootloader_info_t bl = {0};

    /* Filled even when unsupported (reads as "not present"). */
    (void) rs_ota_fw_update_over_ble_BootloaderInfoGet(&bl);

    OTA_LOG("\r\n" RULE "[APP]  OTA firmware update over BLE (RA6E2 + DA14531)\r\n");
    ver_print("app          ", &s_ver);
    OTA_LOG("\r\n");

    if (RS_OTA_FW_UPDATE_OVER_BLE_BOOTLOADER_ID_BUNDLED == bl.id)
    {
        OTA_LOG("[APP]  bootloader   v%u.%u.%u (bundled, crc 0x%08X)\r\n",
                (unsigned) bl.major, (unsigned) bl.minor, (unsigned) bl.patch, (unsigned) bl.crc32);
    }
    else if (RS_OTA_FW_UPDATE_OVER_BLE_BOOTLOADER_ID_UNKNOWN == bl.id)
    {
        OTA_LOG("[APP]  bootloader   UNKNOWN (crc 0x%08X)\r\n", (unsigned) bl.crc32);
    }
    else
    {
        OTA_LOG("[APP]  bootloader   not present\r\n");
    }

    if (bl.record_valid)
    {
        OTA_LOG("[APP]  boot record  layout v%u, %u update(s)\r\n",
                (unsigned) bl.record_version, (unsigned) bl.record_sequence);
    }
    else
    {
        OTA_LOG("[APP]  boot record  none (no swap recorded yet)\r\n");
    }

    OTA_LOG("[APP]  reset        %s, %s start\r\n", ota_log_reset_name(), reset_cold() ? "cold" : "warm");

    if (NULL != p_boot)
    {
        uint32_t o = (uint32_t) p_boot->outcome;

        OTA_LOG("[APP]  boot         outcome=%s%s\r\n",
                s_outcome[(o < (sizeof(s_outcome) / sizeof(s_outcome[0]))) ? o : 0U],
                p_boot->trial_pending ? ", ON TRIAL (reverts on next reset unless confirmed)" : "");
    }

    ota_log_slot_image("secondary    ");
    OTA_LOG(RULE);

    if (ota_log_reset_abnormal())
    {
        OTA_LOG("[APP] !! last reset: %s\r\n", ota_log_reset_name());
    }
}

void ota_log_banner_trial(uint32_t gate_ms)
{
    OTA_LOG("\r\n" RULE);
    ver_print("TRIAL BOOT   ", &s_ver);
    OTA_LOG("\r\n");

    /* After the swap the secondary slot holds the image this one replaced -
     * exactly what a rollback would bring back. */
    ota_log_slot_image("rollback to  ");

    if (ota_log_reset_abnormal())
    {
        OTA_LOG("[APP]  health gate  FAILED - last reset was %s, will NOT confirm\r\n", ota_log_reset_name());
    }
    else
    {
        OTA_LOG("[APP]  health gate  %u s - confirms if the app keeps running, else next reset rolls back\r\n",
                (unsigned) (gate_ms / 1000U));
    }

    OTA_LOG(RULE);
}

#endif /* OTA_LOG_ENABLE */
