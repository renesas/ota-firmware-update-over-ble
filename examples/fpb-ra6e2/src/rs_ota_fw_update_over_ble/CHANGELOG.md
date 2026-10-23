# Changelog — ota_fw_update_over_ble

All notable changes to this project are documented here.
Format follows Keep a Changelog; versions use Semantic Versioning.

## [Unreleased]

### Removed
- Host unit tests and the RAM-backed HAL mock (`tests/`, `tests/mocks/`).
- Component `README.md`; the repository root `README.md` is the single reference.
- Unused SEGGER RTT package files (`Examples/`, `Syscalls/`, pack metadata);
  only `RTT/`, `Config/` and the license remain.

### Changed
- Documentation updated for Renesas FSP 6.5.0.
- **The application now integrates through the public API only.** The demo entry
  (`src/app/ota_fw_update_over_ble_app.c`) calls `rs_ota_fw_update_over_ble_Start()`
  and nothing else — no HAL calls and no direct call into the generated BLE demo.
  `Start()` owns BLE bring-up: it goes through `..._hal_transport_open()`, which is
  the only place that references the generated `app_main()`.
- **API behaviour:** `Start()` is non-blocking as documented. The transport brings
  the BLE/SUOTA event loop up on its **own statically-allocated task** (2 KB stack,
  same priority as the former FSP BLE thread), so `Start()` returns once the link is
  armed and the calling thread stays free to poll `StatusGet()`. `Start()` no longer
  erases the staging window.
- Transfer events are now delivered on the transport's BLE task while the
  application calls the API from another thread, so orchestrator state is shared.
  New `..._hal_lock()` / `..._hal_unlock()` seam brackets those accesses (RTOS
  critical section on target, no-ops on a single-threaded host); `s_status` /
  `s_code_size` are `volatile` and user callbacks are never invoked while held.
  `Stop()` is reachable again (it deletes the BLE task).
- New transfer-start seam `..._hal_feed_start()` / `p_on_start` callback: the
  transport raises it once the staging window is prepared, and the orchestrator
  resets the session and reports `IN_PROGRESS`.
- SUOTA glue now routes image bytes and end-of-transfer through the HAL ingress seams
  (`..._hal_feed_block()` / `..._hal_feed_end()`) instead of calling the staging
  store and verifier directly, and maps `..._StatusGet()` onto the SPOTA_SERV_STATUS
  code reported to the client.
- **Staging erase moved out of the boot path.** `..._hal_stage_open()` is no longer
  called from the application entry (`src/app/ota_fw_update_over_ble_app.c`); the
  staging window is now erased once per download, when the SUOTA client selects a
  memory device (transfer start), so a staged image survives a reset and the flash
  is only cycled when a new image is actually about to arrive.
- New HAL entry points `..._hal_stage_erase_block(index)` and
  `..._hal_stage_block_count()`. The erase is driven **one block per BLE main-loop
  pass** from `ble/app_main.c`, with `R_BLE_Execute()` servicing the GTL link in
  between, and `IMAGE_STARTED` (0x10) is notified only after the last block. Erasing
  the whole window inline in the GATT callback keeps interrupts off for ~0.7 s,
  which desyncs the DA14531 GTL/UART link: the following notification then fails
  with `BLE_ERR_NOT_FOUND` (0x000D) and the phone reports a GATT error.
  `..._hal_stage_open()` (erase everything) is retained but is only safe while the
  BLE link is down.
- **SEGGER RTT logging removed** from the application/BLE demo code. All trace
  output now goes through the `OTA_LOG()` / `OTA_LOG_HEX()` macros in
  `src/app/ota_fw_update_over_ble_log.h`, which currently compile to nothing.
  A VCOM (USB/UART) back-end will be plugged into that header later; it must be
  non-blocking, otherwise a write from the BLE thread stalls `R_BLE_Execute()`
  and the SUOTA session times out. The SUOTA event hex-dumps and the per-chunk
  Patch Data trace were deleted outright (hot path).
- **API break:** `rs_ota_fw_update_over_ble_StatusGet()` now returns `rs_status_t`
  like the rest of the API and reports the workflow state through an out-parameter:
  `rs_status_t ..._StatusGet(rs_ota_fw_update_over_ble_result_t *p_result)`.
  It also fills `bytes_received` / `code_size`; `p_result == NULL` returns
  `RS_STATUS_ERR_ASSERTION`.
- Staging window moved to **0x00028000..0x0003FFFF (96 KB, 3 x 32 KB blocks)**; was
  0x00020000..0x0003FFFF (128 KB). `..._CFG_STAGE_ADDR` = `0x00028000`,
  `..._CFG_STAGE_SIZE` = `0x00018000`. The on-wire image (64-byte SUOTA header +
  code) must now be <= 96 KB total, i.e. a raw payload <= 98 240 bytes — the
  bundled `test_120kb.bin` / `test_128kb.bin` no longer fit.
- Relocated into the project source tree as `src/rs_ota_fw_update_over_ble/` (the
  former per-feature folder level was dropped). The authored demo moved to
  `src/app/`. Only build include paths changed; no source `#include` line changed.
- Restructured into the standard layout: `include/`, `src/`, `src/hal/`,
  `tests/`, `tests/mocks/`. The former `code/` level was removed and `test/`
  renamed to `tests/`; include paths are the only thing that moved, no source
  `#include` line changed.
- All FSP / BLE-stack / SEGGER dependencies are now confined to the single
  translation unit `src/hal/rs_ota_fw_update_over_ble_hal_ra6e2.c`.
  The orchestrator, the dependency check and the verifier are target-agnostic
  and use `rs_status_t` only (no `fsp_err_t` leakage).
- Internal modules folded into the HAL: `rs_fw_store` (code-flash staging) and
  `rs_da14531` (BLE/SUOTA transport seam) are now
  `rs_ota_fw_update_over_ble_hal_*` functions.
- `rs_fw_verify` renamed to `rs_ota_fw_update_over_ble_verify`; it reads staged
  bytes through `..._hal_stage_read()` instead of memory-mapped flash, so it runs
  on a host.
- Board presets (`config/..._data_ra6e2.h`) merged into
  `src/hal/rs_ota_fw_update_over_ble_deps.h`; the reference configuration
  object moved into the HAL source.
- Host tests now link `tests/mocks/mock_fsp_ota_fw_update_over_ble.c`, a RAM-backed
  HAL mock, and cover verifier CRC/header cases and staging overflow.

### Added
- **Bootloader seam** `src/hal/rs_ota_fw_update_over_ble_boot.h` with the three
  placeholder APIs the install phase will use:
  `swap_and_reset(BOOT_SWAP_TYPE_PERM)` (after writing to `BOOT_SECONDARY_BASE`),
  `confirm_and_reset()` (trial swaps only) and `last_result(&result)` (what the
  bootloader did on this boot). Types: `boot_swap_type_t` (`NONE`/`TEST`/`PERM`/
  `REVERT`/`FAIL`), `boot_result_t`, `boot_result_code_t`; slot macros
  `BOOT_SECONDARY_BASE` / `BOOT_SECONDARY_SIZE` / `BOOT_PRIMARY_BASE` default to
  the staging-window presets.
  Default back-end `src/hal/rs_ota_fw_update_over_ble_boot_stub.c` checks
  arguments, traces the intent and returns `RS_STATUS_ERR_UNSUPPORTED` — no flash
  writes, no reset. Selectable from CMake via `RS_OTA_FW_UPDATE_OVER_BLE_BOOT`.
- `rs_ota_fw_update_over_ble_Swap(mode)` **placeholder** for the future
  bootloader/install phase, with `rs_ota_fw_update_over_ble_swap_mode_t`
  (`NOW`, `AFTER_RESET`, `AFTER_RESET_WITH_TEST`, `CONFIRM`, `CANCEL`). It now
  maps onto the bootloader seam (`PERM` / `TEST` / confirm / cancel) and returns
  `RS_STATUS_ERR_UNSUPPORTED` while the stub back-end is linked.
- `RS_STATUS_ERR_UNSUPPORTED` (`-5`) public status code.
- `RS_STATUS_ERR_OVERFLOW` public status code for staging-window overflow.
- `CMakeLists.txt` building the component as a static library with a selectable HAL.
- This changelog.

## [0.1.0]

### Added
- Initial release: acquire a firmware image over BLE (SUOTA via DA14531) and stage it
  into RA6E2 internal code flash; public API `Start` / `Stop` / `StatusGet`.
- Renamed from `fw_acquisition_from_da14531` to `ota_fw_update_over_ble`.
