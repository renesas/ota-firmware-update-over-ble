#!/usr/bin/env bash
# Detects the tools used by the VS Code / CMake build and stores them in the shell
# profile. Not needed for e2studio.
set -u
ok()   { printf '  [OK]   %s\n' "$1"; }
warn() { printf '  [WARN] %s\n' "$1"; }

cd "$(dirname "$0")"
echo ""; echo "ra6e2_da14531ebz_v2 - VS Code environment setup"; echo ""
ALL_FOUND=1
PROFILE="$HOME/.profile"; [ -w "$PROFILE" ] || PROFILE="$HOME/.bashrc"
declare -A RESULT

# newest file called $1 below the remaining roots (optionally filtered by path glob $FILTER)
find_newest() {
    local name="$1"; shift
    local r
    for r in "$@"; do
        [ -d "$r" ] || continue
        find "$r" -maxdepth 7 -type f -name "$name" -path "${FILTER:-*}" 2>/dev/null
    done | sort -V | tail -n1
}

echo "1) LLVM (ATfE) toolchain..."
CLANG=$(FILTER="*ATfE*/bin/clang" find_newest clang "$HOME/.local/share/renesas" "$HOME/renesas" "$HOME/.renesas" /opt/renesas /opt /usr/local)
if [ -n "$CLANG" ] && [ -x "$CLANG" ]; then RESULT[ARM_TOOLCHAIN_PATH]="$(dirname "$CLANG")"; ok "ARM_TOOLCHAIN_PATH = ${RESULT[ARM_TOOLCHAIN_PATH]}"
else ALL_FOUND=0; warn "ATfE not found."; fi

echo "2) GNU Arm toolchain (fallback kit)..."
GCC="$(command -v arm-none-eabi-gcc 2>/dev/null || true)"
[ -z "$GCC" ] && GCC=$(find_newest arm-none-eabi-gcc "$HOME/.renesas/platform/arm-gnu" "$HOME" /opt /usr/local)
if [ -n "$GCC" ] && [ -x "$GCC" ]; then RESULT[ARM_GCC_TOOLCHAIN_PATH]="$(dirname "$GCC")"; ok "ARM_GCC_TOOLCHAIN_PATH = ${RESULT[ARM_GCC_TOOLCHAIN_PATH]}"
else warn "arm-none-eabi-gcc not found (only needed for the 'ARM GCC' kit)."; fi

echo "3) Arm GNU debugger (gdb)..."
GDB=""
[ -n "$GCC" ] && [ -x "$(dirname "$GCC")/arm-none-eabi-gdb" ] && GDB="$(dirname "$GCC")/arm-none-eabi-gdb"
[ -z "$GDB" ] && GDB="$(command -v arm-none-eabi-gdb 2>/dev/null || true)"
[ -z "$GDB" ] && GDB=$(find_newest arm-none-eabi-gdb "$HOME" /opt /usr/local /usr)
if [ -n "$GDB" ] && [ -x "$GDB" ]; then RESULT[ARM_GDB_PATH]="$GDB"; ok "ARM_GDB_PATH = $GDB"
else ALL_FOUND=0; warn "arm-none-eabi-gdb not found (needed for debugging)."; fi

echo "4) Ninja..."
NINJA="$(command -v ninja 2>/dev/null || true)"
[ -z "$NINJA" ] && NINJA=$(find_newest ninja "$HOME/.renesas/platform" "$HOME" /opt /usr/local)
if [ -n "$NINJA" ] && [ -x "$NINJA" ]; then RESULT[NINJA_PATH]="$(dirname "$NINJA")"; ok "NINJA_PATH = ${RESULT[NINJA_PATH]}"
else ALL_FOUND=0; warn "ninja not found."; fi

echo "5) FSP Smart Configurator (RASC)..."
FSP=$(sed -n 's/.*#FSPVersion#" value="\([0-9.]*\)".*/\1/p' configuration.xml 2>/dev/null | head -n1)
RASC=""
[ -n "$FSP" ] && RASC=$(FILTER="*fsp_${FSP}*" find_newest rasc "$HOME/.renesas/platform/sc" "$HOME/.local/share/renesas" /opt/renesas)
[ -z "$RASC" ] && RASC=$(find_newest rasc "$HOME/.renesas/platform/sc" "$HOME/.local/share/renesas" /opt/renesas)
if [ -n "$RASC" ] && [ -x "$RASC" ]; then RESULT[RASC_EXE_PATH]="$RASC"; ok "RASC_EXE_PATH = $RASC"
else ALL_FOUND=0; warn "rasc not found. Install the FSP ${FSP} Smart Configurator."; fi

BEGIN="# >>> ra6e2_da14531ebz_v2 env >>>"; END="# <<< ra6e2_da14531ebz_v2 env <<<"
if [ ${#RESULT[@]} -gt 0 ]; then
    if [ -f "$PROFILE" ] && grep -qF "$BEGIN" "$PROFILE"; then
        tmp="$(mktemp)"
        awk -v b="$BEGIN" -v e="$END" '$0==b{skip=1;next} skip&&$0==e{skip=0;next} !skip{print}' "$PROFILE" > "$tmp" && mv "$tmp" "$PROFILE"
    fi
    { echo "$BEGIN"; for k in "${!RESULT[@]}"; do printf 'export %s="%s"\n' "$k" "${RESULT[$k]}"; done; echo "$END"; } >> "$PROFILE"
fi

echo ""
[ "$ALL_FOUND" -eq 1 ] && ok "All required tools configured." || warn "Some tools are missing (see above)."
echo "Variables written to: $PROFILE"
echo "IMPORTANT: log out/in (or 'source $PROFILE') and restart VS Code."
