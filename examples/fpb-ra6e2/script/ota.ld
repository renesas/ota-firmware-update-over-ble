/*
                  Linker File for Renesas FSP - OTA swap-bootloader build
                  (ra6e2_da14531ebz_v2, FPB-RA6E2)

    WHY THIS FILE EXISTS
    --------------------
    "Generate Project Content" rewrites BOTH generated linker inputs:
        <config>/memory_regions.ld   - whole-device flash and RAM
        script/fsp.ld                - the stock two-line include
    so an override placed in either of them is silently reverted the next time the
    configurator runs, and the build then succeeds while producing an image that
    cannot boot. RASC has no template for THIS file, so it is left alone.

    Select it in Properties -> C/C++ Build -> Settings -> Linker -> Script file,
    for the Debug AND Release configurations. script/fsp.ld is then unused and may
    be regenerated freely.

    WHAT IT CHANGES
    ---------------
    1. Link address. This application runs from the bootloader's PRIMARY slot, not
       from address 0 (where the bootloader itself lives). It is linked ONCE, for
       the primary slot: a staged image is physically moved into the primary slot
       by the bootloader before it ever executes, so one build works regardless of
       which slot it was downloaded into.

           FLASH_START  = 0x00010000   (BOOT_PRIMARY_BASE)
           FLASH_LENGTH = 0x00018000   (BOOT_PRIMARY_SIZE, 96 KB)

    2. Mailbox reservation. The top 32 bytes of SRAM carry the request/result
       handshake with the bootloader across a reset. Trimming RAM_LENGTH keeps
       .bss, the heap and the stack out of that region, and keeps startup from
       zeroing it.

    Flash map (master copy: ota_fw_bootloader/src/boot_layout.h):

        0x00000000  16 KB   bootloader      <- embedded in this image, see (3)
        0x00004000  16 KB   spare
        0x00008000  32 KB   scratch
        0x00010000  96 KB   primary slot    <- this image is linked here
        0x00028000  96 KB   secondary slot  <- SUOTA staging window
        0x08000000  128 B   boot record + swap log (data flash, VEE starts above)
*/

INCLUDE memory_regions.ld

/* Run from the primary slot instead of the start of flash. Assigned AFTER the
 * generated file is included and BEFORE fsp_gen.ld, which is where the MEMORY
 * block reads these symbols. */
FLASH_START  = 0x00010000;
FLASH_LENGTH = 0x00018000;

/* Reserve the top of SRAM for the boot mailbox. Must match RSBOOT_MAILBOX_RESERVE
 * in src/rs_ota_fw_update_over_ble/src/hal/rs_ota_fw_update_over_ble_boot_rsboot.c
 * and the identical line in the bootloader's linker script, or the two images
 * disagree about where the mailbox is and every request is silently ignored. */
RAM_LENGTH = RAM_LENGTH - 0x20;

INCLUDE fsp_gen.ld

/*  3. Embedded bootloader.

    The application does not run from address 0 - the bootloader does. Rather than
    asking every developer to remember a separate one-off programming step for it,
    the pre-built bootloader image is linked into THIS image at its real address,
    so a single download of the .elf / .srec programs both.

    The bytes come from src/rs_ota_fw_update_over_ble/bootloader.bin via the
    .incbin at the top of src/hal/rs_ota_fw_update_over_ble_boot_rsboot.c. Nothing
    in the application references them, so KEEP() is required or --gc-sections
    drops the section without a word.

    No ">FLASH": that region starts at 0x00010000 (see above), while this section
    belongs below it. The address is therefore given explicitly.

    makefile.targets strips this section from the _ota artifacts - an OTA payload
    must contain the application only. */
SECTIONS
{
    .bootloader 0x00000000 :
    {
        KEEP(*(.bootloader))
    }
}

/* 0x4000 = BOOT_BOOTLOADER_SIZE in ota_fw_bootloader/src/boot_layout.h. Overflowing
 * it would silently corrupt the spare region that follows. */
ASSERT(SIZEOF(.bootloader) <= 0x4000,
       "Embedded bootloader exceeds BOOT_BOOTLOADER_SIZE (16 KB) - it would overrun the spare region at 0x00004000")

/* A wrong link address still links cleanly and only fails once it is on the
 * board, so fail here instead. */
ASSERT(FLASH_START == 0x00010000,
       "Application must be linked at BOOT_PRIMARY_BASE (0x00010000) - is script/ota.ld selected as the linker script?")
ASSERT(FLASH_LENGTH == 0x00018000,
       "Application must be limited to BOOT_PRIMARY_SIZE (96 KB) - a larger image would overrun the secondary slot")
