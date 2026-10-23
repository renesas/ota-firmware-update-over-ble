/******************************************************************************
 * File Name    : rs_ota_fw_update_over_ble_boot_rsboot.c
 * Description  : Renesas RA swap-bootloader back-end for the seam declared in
 *                hal/rs_ota_fw_update_over_ble_boot.h.
 *
 *                Companion bootloader: ota_fw_bootloader (FPB-RA6E2). It owns the
 *                flash map, the boot record, the sector exchange, image validation
 *                and resume-after-power-cut. This file owns NONE of that: it only
 *                posts a request into a fixed RAM mailbox and (optionally) resets.
 *                The bootloader reads the mailbox out of reset, acts on it, and
 *                writes back what it did.
 *
 *                Division of labour:
 *                  - this RS stages an image at BOOT_SECONDARY_BASE
 *                  - swap_arm() / swap_and_reset() ask for it to be installed
 *                  - the bootloader validates it and moves it
 *
 *                A rejected request costs one reboot and nothing else: the board
 *                comes back up on the image it was already running and
 *                last_result() reports BOOT_RESULT_BAD_IMAGE.
 *
 *                SYNCHRONISATION: the mailbox layout below is a COPY of the
 *                bootloader's src/boot_mailbox.h, which is the master. The two
 *                projects build separately and cannot share a header, so the
 *                bootloader's script/check_contract.ps1 compares them and fails on
 *                drift. Run it after changing either side. A silent mismatch is
 *                indistinguishable from "the swap never happened".
 ******************************************************************************/
#include "rs_ota_fw_update_over_ble_deps.h"

/*******************************************************************************
 * The bundled bootloader image itself
 *
 * bootloader.bin is NOT a source file: no compiler, no linker and no makefile
 * rule looks at it. Without the .incbin below the bin is simply ignored, the
 * linked image contains the application only, and address 0x00000000 keeps
 * whatever happened to be programmed there before (or stays erased after a mass
 * erase - i.e. an unbootable board).
 *
 * .incbin pastes the raw bytes into a normal input section, ".bootloader", which
 * script/ota.ld then places at 0x00000000. The result is that ONE download of
 * the main .elf / .srec programs bootloader + application together, and the
 * bootloader needs no separate manual programming step.
 *
 * FROZEN: the bin is committed and treated as ROM content - no build-time
 * regeneration, no dependency on the ota_fw_bootloader project being built. To
 * refresh it, copy it in by hand and update the label below (see README).
 *
 * NOT PART OF THE OTA PAYLOAD: makefile.targets strips ".bootloader" from
 * <name>_ota.bin. An OTA image is staged into the secondary
 * slot and swapped by the bootloader that is ALREADY running - shipping a copy
 * of the bootloader inside it would be dead weight at best, and would land at
 * the wrong address at worst.
 *
 * Deliberately OUTSIDE the CFG_USE_RS_BOOTLOADER guard below: turning the
 * back-end off must not silently produce an image with no bootloader in it.
 *
 * PATH: .incbin resolves relative to the assembler's working directory, which
 * for this project is the build configuration folder (Debug/ or Release/) -
 * hence the leading "../". This avoids adding an -Wa,-I include path to the
 * project settings, which "Generate Project Content" could revert.
 *
 * "a" = allocatable, %progbits = occupies space in the file (as opposed to
 * %nobits like .bss), so the bytes are actually emitted into the ELF and reach
 * the flash. KEEP() in the linker script stops --gc-sections from discarding
 * the section, which it otherwise would: nothing here references these bytes.
 ******************************************************************************/
__asm__ (".section .bootloader,\"a\",%progbits\n"
         ".balign 4\n"
         ".global g_bootloader_image\n"
         "g_bootloader_image:\n"
         ".incbin \"../src/rs_ota_fw_update_over_ble/bootloader.bin\"\n"
         /* End marker, BEFORE the padding: g_bootloader_image_end - g_bootloader_image
          * is the exact blob length (visible in the .map, handy in a debugger). */
         ".global g_bootloader_image_end\n"
         "g_bootloader_image_end:\n"
         ".balign 4\n"
         ".previous\n");

#if (RS_OTA_FW_UPDATE_OVER_BLE_CFG_USE_RS_BOOTLOADER)

#include "bsp_api.h"                         /* NVIC_SystemReset(), __DMB/__DSB     */
#include "rs_ota_fw_update_over_ble_boot.h"

/*******************************************************************************
 * Bundled bootloader label - private to this file
 *
 * bootloader.bin carries no version stamp, so it is identified by FINGERPRINT:
 * the CRC32 of its first BLOB_SIZE bytes in flash. Match -> "v1.0.0 (bundled)",
 * anything else -> "UNKNOWN (crc 0x...)".
 *
 * WHEN bootloader.bin IS REPLACED: run tools refresh_bootloader_blob.ps1 at the
 * repo root. It copies the freshly built Debug/bootloader.bin in and prints the
 * two lines below ready to paste - the CRC32 is computed on the host (IEEE
 * 802.3, self-tested against the standard check vector), so there is no need to
 * flash once and read the value back off the banner.
 *
 * v1.1.0: the bootloader updated to v1.1.0
 ******************************************************************************/
#define RSBOOT_BL_BLOB_MAJOR         (1U)
#define RSBOOT_BL_BLOB_MINOR         (1U)
#define RSBOOT_BL_BLOB_PATCH         (0U)
#define RSBOOT_BL_BLOB_SIZE          (7708U)
#define RSBOOT_BL_BLOB_CRC32         (0xE9645C84UL)

/* Bootloader region (copy of ota_fw_bootloader/src/boot_layout.h). */
#define RSBOOT_BL_BASE               (0x00000000UL)
#define RSBOOT_BL_SIZE               (0x00004000UL)  /* 16 KB */

/*******************************************************************************
 * Mailbox - private to this file
 *
 * The application and the bootloader never run at the same time, so the request
 * is handed over across a reset through a fixed RAM address. NVIC_SystemReset()
 * does not clear SRAM, and the linker reserves this region in both images
 * (script/ota.ld trims RAM_LENGTH), so nothing else can land on it and startup
 * does not zero it.
 *
 * The request does NOT survive a power cycle - only a warm reset preserves RAM.
 ******************************************************************************/

#define RSBOOT_RAM_BASE              (0x20000000UL)
#define RSBOOT_RAM_SIZE              (0x0000A000UL) /* 40 KB */

/** Bytes carved off the top of RAM for the mailbox. Must match the
 *  "RAM_LENGTH - ..." line in script/ota.ld. */
#define RSBOOT_MAILBOX_RESERVE       (0x20UL)

#define RSBOOT_MAILBOX_ADDRESS       (RSBOOT_RAM_BASE + RSBOOT_RAM_SIZE - RSBOOT_MAILBOX_RESERVE)

/** 'B','O','R','Q' little endian - present only while a request is pending. */
#define RSBOOT_MAILBOX_MAGIC         (0x51524F42UL)

/** 'B','A','C','K' little endian - present once the bootloader has reported the
 *  outcome of the last boot. */
#define RSBOOT_ACK_MAGIC             (0x4B434142UL)

/** Requests the bootloader understands (bootloader boot_command_t). */
#define RSBOOT_CMD_SWAP              (1U)
#define RSBOOT_CMD_CONFIRM           (2U)

/** Swap types as the BOOTLOADER numbers them. Deliberately different from this
 *  RS's boot_swap_type_t (TEST=1, PERM=2) - see rsboot_swap_arg(). */
#define RSBOOT_SWAP_PERM             (1U)
#define RSBOOT_SWAP_TEST             (2U)

/** Outcomes the bootloader reports (bootloader boot_status_t). */
#define RSBOOT_STATUS_OK             (1U)
#define RSBOOT_STATUS_SWAPPED        (2U)
#define RSBOOT_STATUS_CONFIRMED      (3U)
#define RSBOOT_STATUS_REVERTED       (4U)
#define RSBOOT_STATUS_ERR_IMAGE      (5U)
#define RSBOOT_STATUS_ERR_WRITE      (6U)
#define RSBOOT_STATUS_ERR_REQUEST    (7U)
#define RSBOOT_STATUS_ERR_SWAP       (8U)
#define RSBOOT_STATUS_ERR_CRC        (9U)

/** 32-byte RAM handshake: a request written here, a result written by the
 *  bootloader. Field-for-field copy of the bootloader's boot_mailbox_t. */
typedef struct st_rsboot_mailbox
{
    /* Request - written here, invalidated by the bootloader once consumed. */
    uint32_t magic;                    /* RSBOOT_MAILBOX_MAGIC while pending      */
    uint8_t  command;                  /* RSBOOT_CMD_*                            */
    uint8_t  arg;                      /* RSBOOT_CMD_SWAP: RSBOOT_SWAP_*          */
    uint8_t  reserved[2];              /* padding                                 */
    uint32_t check;                    /* integrity check over command + arg      */

    /* Result - written by the bootloader on every boot, never cleared by it. */
    uint32_t ack_magic;                /* RSBOOT_ACK_MAGIC once filled            */
    uint8_t  status;                   /* RSBOOT_STATUS_*                         */
    uint8_t  req_cmd;                  /* command that caused this boot, or 0xFF  */
    uint8_t  detail;                   /* swap legs completed                     */
    uint8_t  ack_reserved;             /* padding                                 */
    uint32_t ack_check;                /* integrity check over the result fields  */

    uint32_t spare[2];                 /* keeps the struct at 32 bytes            */
} rsboot_mailbox_t;

#define RSBOOT_MAILBOX               (*(volatile rsboot_mailbox_t *) RSBOOT_MAILBOX_ADDRESS)

/*******************************************************************************
 * Trial marker - RS-private, in spare[0]
 *
 * The bootloader's acknowledgement reports WHAT happened (swapped / reverted /
 * rejected) but not whether the swap it performed was a trial, so "am I on trial
 * right now?" cannot be answered from the acknowledgement alone.
 *
 * spare[] is the one part of the mailbox the bootloader never reads or writes, so
 * this file uses spare[0] as its own note-to-self across the reset: written when a
 * trial swap is armed, cleared on confirm/permanent/cancel. It is a cookie rather
 * than a flag so that uninitialised SRAM after a cold power-on does not read as
 * "on trial". This is RS-private state and costs the bootloader nothing.
 ******************************************************************************/
#define RSBOOT_TRIAL_COOKIE          (0x54524C31UL) /* 'T','R','L','1' */

/** Integrity value over command + arg. Rejects random SRAM contents after a cold
 *  power-on. Must match the bootloader's boot_mailbox_check(). */
static uint32_t rsboot_check (uint8_t command, uint8_t arg)
{
    return RSBOOT_MAILBOX_MAGIC ^ 0xA5A5A5A5UL ^ ((uint32_t) command << 8) ^ (uint32_t) arg;
}

/** Integrity value over the result fields. Must match the bootloader's
 *  boot_mailbox_ack_check(). */
static uint32_t rsboot_ack_check (uint8_t status, uint8_t req_cmd, uint8_t detail)
{
    return RSBOOT_ACK_MAGIC ^ 0x5A5A5A5AUL ^ ((uint32_t) status << 16) ^
           ((uint32_t) req_cmd << 8) ^ (uint32_t) detail;
}

/**
 * Fill the mailbox. Payload first, magic last, so the bootloader can never see a
 * half-written request.
 *
 * Posting does NOT itself trigger anything: the bootloader only looks at the
 * mailbox out of reset. The request stays pending until the next reset, whatever
 * causes it.
 */
static void rsboot_post (uint8_t command, uint8_t arg, bool trial)
{
    volatile rsboot_mailbox_t * p_mail = &RSBOOT_MAILBOX;

    p_mail->command     = command;
    p_mail->arg         = arg;
    p_mail->reserved[0] = 0U;
    p_mail->reserved[1] = 0U;
    p_mail->check       = rsboot_check(command, arg);
    p_mail->spare[0]    = trial ? RSBOOT_TRIAL_COOKIE : 0UL;

    __DMB();

    p_mail->magic = RSBOOT_MAILBOX_MAGIC;

    __DSB();
}

/** Discard a pending request. Clearing the magic is enough - the bootloader
 *  ignores everything else without it. */
static void rsboot_cancel (void)
{
    volatile rsboot_mailbox_t * p_mail = &RSBOOT_MAILBOX;

    p_mail->magic    = 0UL;
    p_mail->check    = 0UL;
    p_mail->spare[0] = 0UL;

    __DSB();
}

/** True when this file previously armed a TRIAL swap that has since been
 *  installed, i.e. the running image will be reverted unless it confirms. */
static bool rsboot_trial_marked (void)
{
    return (RSBOOT_TRIAL_COOKIE == RSBOOT_MAILBOX.spare[0]);
}

/** Translate this RS's swap type to the bootloader's numbering. The two enums
 *  use DIFFERENT values for the same thing, so never cast between them. */
static rs_status_t rsboot_swap_arg (boot_swap_type_t swap_type, uint8_t * p_arg, bool * p_trial)
{
    switch (swap_type)
    {
        case BOOT_SWAP_TYPE_PERM:
            *p_arg   = RSBOOT_SWAP_PERM;
            *p_trial = false;
            return RS_STATUS_SUCCESS;

        case BOOT_SWAP_TYPE_TEST:
            *p_arg   = RSBOOT_SWAP_TEST;
            *p_trial = true;
            return RS_STATUS_SUCCESS;

        case BOOT_SWAP_TYPE_REVERT:
        case BOOT_SWAP_TYPE_FAIL:
            /* The bootloader decides on its own when to revert (an unconfirmed
             * trial image) and when a swap is impossible; neither can be demanded
             * from here. */
            return RS_STATUS_ERR_UNSUPPORTED;

        case BOOT_SWAP_TYPE_NONE:
        default:
            return RS_STATUS_ERR_ASSERTION;
    }
}

/*******************************************************************************
 * Seam implementation
 ******************************************************************************/

rs_status_t swap_arm (boot_swap_type_t swap_type)
{
    if (BOOT_SWAP_TYPE_NONE == swap_type)
    {
        rsboot_cancel();
        return RS_STATUS_SUCCESS;
    }

    uint8_t     arg   = 0U;
    bool        trial = false;
    rs_status_t rc    = rsboot_swap_arg(swap_type, &arg, &trial);

    if (RS_STATUS_SUCCESS != rc)
    {
        return rc;
    }

    rsboot_post((uint8_t) RSBOOT_CMD_SWAP, arg, trial);

    return RS_STATUS_SUCCESS;
}

rs_status_t swap_and_reset (boot_swap_type_t swap_type)
{
    rs_status_t rc = swap_arm(swap_type);

    if (RS_STATUS_SUCCESS != rc)
    {
        return rc;
    }

    if (BOOT_SWAP_TYPE_NONE == swap_type)
    {
        /* Cancelling means "keep running what is running" - resetting would be
         * the opposite of what was asked for. */
        return RS_STATUS_SUCCESS;
    }

    NVIC_SystemReset();

    /* Not reached. */
    while (1)
    {
        __NOP();
    }
}

rs_status_t confirm_and_reset (void)
{
    rsboot_post((uint8_t) RSBOOT_CMD_CONFIRM, 0U, false);

    NVIC_SystemReset();

    /* Not reached. */
    while (1)
    {
        __NOP();
    }
}

rs_status_t last_result (boot_result_t * p_result)
{
    if (NULL == p_result)
    {
        return RS_STATUS_ERR_ASSERTION;
    }

    /* Always leave the caller with a defined struct. */
    p_result->code            = BOOT_RESULT_UNKNOWN;
    p_result->swap_type       = BOOT_SWAP_TYPE_NONE;
    p_result->image_confirmed = false;
    p_result->image_size      = 0U;   /* the bootloader does not track image size */

    const volatile rsboot_mailbox_t * p_mail = &RSBOOT_MAILBOX;

    if (RSBOOT_ACK_MAGIC != p_mail->ack_magic)
    {
        /* No report: started some other way, or SRAM was not preserved (cold
         * power-on). Not an error - just nothing to say. */
        return RS_STATUS_SUCCESS;
    }

    uint8_t status  = p_mail->status;
    uint8_t req_cmd = p_mail->req_cmd;
    uint8_t detail  = p_mail->detail;

    if (p_mail->ack_check != rsboot_ack_check(status, req_cmd, detail))
    {
        return RS_STATUS_SUCCESS;
    }

    bool trial = rsboot_trial_marked();

    switch (status)
    {
        case RSBOOT_STATUS_SWAPPED:
            p_result->code            = BOOT_RESULT_SWAPPED;
            p_result->swap_type       = trial ? BOOT_SWAP_TYPE_TEST : BOOT_SWAP_TYPE_PERM;
            /* A permanent swap is final the moment it completes; a trial one is
             * not confirmed until confirm_and_reset() has been through a reset. */
            p_result->image_confirmed = !trial;
            break;

        case RSBOOT_STATUS_REVERTED:
            p_result->code      = BOOT_RESULT_REVERTED;
            p_result->swap_type = BOOT_SWAP_TYPE_REVERT;
            break;

        case RSBOOT_STATUS_CONFIRMED:
            p_result->code            = BOOT_RESULT_NONE;
            p_result->image_confirmed = true;
            break;

        case RSBOOT_STATUS_ERR_IMAGE:
        case RSBOOT_STATUS_ERR_CRC:
            /* Nothing usable was installed: either the staged image was rejected
             * before the exchange, or the exchange finished and the result did not
             * match the image that was staged. */
            p_result->code      = BOOT_RESULT_BAD_IMAGE;
            p_result->swap_type = BOOT_SWAP_TYPE_FAIL;
            break;

        case RSBOOT_STATUS_ERR_WRITE:
        case RSBOOT_STATUS_ERR_REQUEST:
        case RSBOOT_STATUS_ERR_SWAP:
            /* The request was refused or the exchange broke down. The board is
             * running SOMETHING valid (the bootloader checks before jumping), but
             * it is not the staged image. */
            p_result->code      = BOOT_RESULT_BAD_IMAGE;
            p_result->swap_type = BOOT_SWAP_TYPE_FAIL;
            break;

        case RSBOOT_STATUS_OK:
        default:
            p_result->code            = BOOT_RESULT_NONE;
            p_result->image_confirmed = !trial;
            break;
    }

    return RS_STATUS_SUCCESS;
}

/*******************************************************************************
 * Read-only identification: bootloader, boot record, secondary slot
 *
 * Everything below only READS flash. Code flash and (on FLASH_HP parts) data
 * flash are memory mapped, so no driver is involved - but nothing here may run
 * while the flash is in program/erase mode, which is why the application asks
 * before it starts a transfer.
 ******************************************************************************/

/** Boot record in data flash block 0 (copy of the bootloader's boot_layout.h). */
#define RSBOOT_RECORD_ADDRESS        (0x08000000UL)
#define RSBOOT_RECORD_SIZE           (64U)
#define RSBOOT_RECORD_MAGIC          (0x544F4F42UL) /* 'B','O','O','T' */
#define RSBOOT_RECORD_CRC_LENGTH     (RSBOOT_RECORD_SIZE - 2U)

/**
 * Pointer to a flash address that may be 0.
 *
 * The bootloader lives at 0x00000000, and a constant (T *) 0 is a NULL pointer
 * to the compiler: at -O2 GCC is entitled to treat the dereference as undefined
 * and replace it with a trap (-fisolate-erroneous-paths-dereference). The empty
 * asm hides the value from the optimiser, so the load is emitted as written.
 */
static const volatile uint8_t * rsboot_flash (uint32_t address)
{
    uintptr_t a = (uintptr_t) address;
    __asm__ volatile ("" : "+r" (a));

    return (const volatile uint8_t *) a;
}

static uint32_t rsboot_read32 (uint32_t address)
{
    const volatile uint8_t * p = rsboot_flash(address);

    return (uint32_t) p[0] | ((uint32_t) p[1] << 8) | ((uint32_t) p[2] << 16) | ((uint32_t) p[3] << 24);
}

/** CRC32, IEEE 802.3 (reflected 0xEDB88320) - matches zlib / PowerShell
 *  tooling. Bitwise: runs once per boot over ~8 KB. */
static uint32_t rsboot_crc32 (uint32_t address, uint32_t length)
{
    const volatile uint8_t * p   = rsboot_flash(address);
    uint32_t                 crc = 0xFFFFFFFFUL;

    for (uint32_t i = 0U; i < length; i++)
    {
        crc ^= (uint32_t) p[i];

        for (uint32_t bit = 0U; bit < 8U; bit++)
        {
            crc = (0U != (crc & 1U)) ? ((crc >> 1) ^ 0xEDB88320UL) : (crc >> 1);
        }
    }

    return crc ^ 0xFFFFFFFFUL;
}

/** CRC16-CCITT (poly 0x1021, init 0xFFFF) - the bootloader's boot_record_crc16(). */
static uint16_t rsboot_crc16 (uint32_t address, uint32_t length)
{
    const volatile uint8_t * p   = rsboot_flash(address);
    uint16_t                 crc = 0xFFFFU;

    for (uint32_t i = 0U; i < length; i++)
    {
        crc ^= (uint16_t) ((uint16_t) p[i] << 8);

        for (uint32_t bit = 0U; bit < 8U; bit++)
        {
            crc = (0U != (crc & 0x8000U)) ? (uint16_t) (((uint16_t) (crc << 1)) ^ 0x1021U)
                                          : (uint16_t) (crc << 1);
        }
    }

    return crc;
}

/** Same checks the bootloader's boot_image_ok_at() applies: initial SP inside
 *  RAM and 8-byte aligned, reset handler a Thumb address inside the region the
 *  image is linked for. */
static bool rsboot_vectors_ok (uint32_t base, uint32_t link_base, uint32_t size)
{
    uint32_t sp = rsboot_read32(base);
    uint32_t pc = rsboot_read32(base + 4U);

    if ((0xFFFFFFFFUL == sp) || (0xFFFFFFFFUL == pc))
    {
        return false;
    }

    if ((sp < RSBOOT_RAM_BASE) || (sp > (RSBOOT_RAM_BASE + RSBOOT_RAM_SIZE)) || (0U != (sp & 0x7U)))
    {
        return false;
    }

    if (0U == (pc & 0x1U))
    {
        return false;
    }

    uint32_t handler = pc & ~0x1UL;

    return (handler >= link_base) && (handler < (link_base + size));
}

rs_status_t bootloader_info (boot_bl_info_t * p_info)
{
    if (NULL == p_info)
    {
        return RS_STATUS_ERR_ASSERTION;
    }

    p_info->source          = BOOT_BL_SOURCE_NONE;
    p_info->major           = 0U;
    p_info->minor           = 0U;
    p_info->patch           = 0U;
    p_info->crc32           = 0U;
    p_info->crc32_length    = 0U;
    p_info->record_valid    = false;
    p_info->record_version  = 0U;
    p_info->record_sequence = 0U;

    /* ---- boot record: independent of how the bootloader is identified ------- */
    if (RSBOOT_RECORD_MAGIC == rsboot_read32(RSBOOT_RECORD_ADDRESS))
    {
        const volatile uint8_t * p_rec  = rsboot_flash(RSBOOT_RECORD_ADDRESS);
        uint16_t                 stored = (uint16_t) ((uint16_t) p_rec[RSBOOT_RECORD_CRC_LENGTH] |
                                                      (uint16_t) ((uint16_t) p_rec[RSBOOT_RECORD_CRC_LENGTH + 1U] << 8));

        if (stored == rsboot_crc16(RSBOOT_RECORD_ADDRESS, RSBOOT_RECORD_CRC_LENGTH))
        {
            p_info->record_valid    = true;
            p_info->record_version  = p_rec[4];
            p_info->record_sequence = rsboot_read32(RSBOOT_RECORD_ADDRESS + 8U);
        }
    }

    /* ---- bootloader --------------------------------------------------------- */
    const uint32_t bl_base = RSBOOT_BL_BASE;
    const uint32_t bl_size = RSBOOT_BL_SIZE;

    if ((0xFFFFFFFFUL == rsboot_read32(bl_base)) && (0xFFFFFFFFUL == rsboot_read32(bl_base + 4U)))
    {
        /* Erased: nothing there. (Only reachable when this image was started by a
         * debugger straight at the primary slot.) */
        return RS_STATUS_SUCCESS;
    }

    /* Fingerprint against the bootloader this application ships with. Taken over
     * the SAME length as the bundled blob, so a match means byte-for-byte
     * identical code. */
    p_info->crc32_length = RSBOOT_BL_BLOB_SIZE;
    p_info->crc32        = rsboot_crc32(bl_base, RSBOOT_BL_BLOB_SIZE);

    if ((RSBOOT_BL_BLOB_CRC32 == p_info->crc32) && rsboot_vectors_ok(bl_base, bl_base, bl_size))
    {
        p_info->source = BOOT_BL_SOURCE_BUNDLED;
        p_info->major  = (uint8_t) RSBOOT_BL_BLOB_MAJOR;
        p_info->minor  = (uint8_t) RSBOOT_BL_BLOB_MINOR;
        p_info->patch  = (uint8_t) RSBOOT_BL_BLOB_PATCH;

        return RS_STATUS_SUCCESS;
    }

    /* Something is there, but not something we can name. */
    p_info->source = BOOT_BL_SOURCE_UNKNOWN;

    return RS_STATUS_SUCCESS;
}

rs_status_t secondary_slot (boot_slot_info_t * p_slot)
{
    if (NULL == p_slot)
    {
        return RS_STATUS_ERR_ASSERTION;
    }

    p_slot->base = (uint32_t) BOOT_SECONDARY_BASE;
    p_slot->size = (uint32_t) BOOT_SECONDARY_SIZE;

    /* A staged image is linked for the PRIMARY slot, so its reset vector points
     * there even while it sits in the secondary one. */
    p_slot->image_present = rsboot_vectors_ok((uint32_t) BOOT_SECONDARY_BASE,
                                              (uint32_t) BOOT_PRIMARY_BASE,
                                              (uint32_t) BOOT_PRIMARY_SIZE);

    return RS_STATUS_SUCCESS;
}

#endif /* RS_OTA_FW_UPDATE_OVER_BLE_CFG_USE_RS_BOOTLOADER */
