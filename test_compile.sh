#!/usr/bin/env bash
#
# test_compile.sh -- test-compiles every example against every supported
# platform (RP2040, ESP32-S3, STM32), using arduino-cli.
#
# Usage:
#   ./test_compile.sh                    # everything
#   ./test_compile.sh --board rp2040      # one platform only
#   ./test_compile.sh --example USBHostMSC_Info   # one example only
#   ./test_compile.sh --list             # show the board/example matrix and exit
#   ./test_compile.sh -v                 # show full arduino-cli output for failures
#
# Requires: arduino-cli, with these cores installed --
#   arduino-cli core install rp2040:rp2040
#   arduino-cli core install esp32:esp32
#   arduino-cli core install STMicroelectronics:stm32
# and these libraries installed (see library.properties' depends= and
# extras/adafruit_tinyusb_stm32_host.patch):
#   TinyFATFS, TinyEXT4 (+ Adafruit TinyUSB Library for the stm32/esp32 rows)
#
# Exit code is 0 if every non-skipped combination compiled cleanly, 1
# otherwise.

set -uo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
EXAMPLES_DIR="$SCRIPT_DIR/examples"
COMPILE_TIMEOUT=300 # seconds, per example/board combination

# ---------------------------------------------------------------------------
# Board matrix: one "key|fqbn|extra arduino-cli args..." row per platform.
# ---------------------------------------------------------------------------
BOARDS=(
  "rp2040|rp2040:rp2040:rpipico:usbstack=tinyusb_host|"
  "rp2040dev|rp2040:rp2040:rpipico:usbstack=tinyusb|"
  "esp32s3|esp32:esp32:esp32s3:USBMode=default|"
  "stm32|STMicroelectronics:stm32:GenF4:pnum=BLACKPILL_F411CE|--build-property compiler.c.extra_flags=-DCFG_TUH_ENABLED=1 --build-property compiler.cpp.extra_flags=-DCFG_TUH_ENABLED=1"
  "esp32|esp32:esp32:esp32|"
)

# ---------------------------------------------------------------------------
# Known-incompatible example/board combinations, skipped rather than run.
# Format: "board_key:ExampleName" -- one reason string per entry.
# ---------------------------------------------------------------------------
declare -A SKIP
SKIP["stm32:USBHostMSC_AutoFS_ListFiles"]="USBMscAutoFS is not ported to STM32 yet (see the wiki page STM32)"

# Classic ESP32 has no native USB peripheral, so every example except
# USBHostMSC_MAX3421E_Info -- which uses the Adafruit_USBH_Host/MAX3421E
# constructor -- would compile clean but sit there doing nothing at
# runtime (the plain USBHostMSC() constructor never wires up SPI/CS/INT).
# See the wiki page MAX3421E.
MAX3421E_REASON="classic ESP32 has no native USB -- needs the MAX3421E constructor (see USBHostMSC_MAX3421E_Info, the wiki page MAX3421E)"
SKIP["esp32:USBHostMSC_Info"]="$MAX3421E_REASON"
SKIP["esp32:USBHostMSC_BlockRW"]="$MAX3421E_REASON"
SKIP["esp32:USBHostMSC_FatFS_ListFiles"]="$MAX3421E_REASON"
SKIP["esp32:USBHostMSC_AutoFS_ListFiles"]="$MAX3421E_REASON"
SKIP["esp32:USBHostMSC_Ext4ReadFile"]="$MAX3421E_REASON"
SKIP["esp32:USBHostMSC_Ext4WriteFile"]="$MAX3421E_REASON"
SKIP["esp32:USBHostMSC_FSPool_HubListFiles"]="$MAX3421E_REASON"

# The reverse: USBHostMSC_MAX3421E_Info uses the Adafruit_USBH_Host
# constructor, which only exists when CFG_TUH_MAX3421 is on -- not the
# default on boards with a native USB controller.
NATIVE_REASON="CFG_TUH_MAX3421 is off by default here -- the MAX3421E constructor doesn't exist (see USBHostMSC_Info instead)"
SKIP["rp2040:USBHostMSC_MAX3421E_Info"]="$NATIVE_REASON"
SKIP["esp32s3:USBHostMSC_MAX3421E_Info"]="$NATIVE_REASON"
SKIP["stm32:USBHostMSC_MAX3421E_Info"]="$NATIVE_REASON"

# USB host and device mode need different USB stacks on RP2040, so the device
# examples (USBDeviceMSC_*) build only on rp2040dev (device stack) and
# esp32s3 (the core has both); the host examples (USBHostMSC_*) skip rp2040dev.
for ex_dir in "$EXAMPLES_DIR"/*/; do
  ex="$(basename "$ex_dir")"
  case "$ex" in
    USBDeviceMSC_*)
      for b in rp2040 stm32 esp32; do
        SKIP["$b:$ex"]="device-mode example; needs a device-capable USB stack (see the wiki page DEVICE_API)"
      done ;;
    *)
      SKIP["rp2040dev:$ex"]="host-mode example; rp2040dev builds the USB device stack" ;;
  esac
done

# ---------------------------------------------------------------------------
# Argument parsing
# ---------------------------------------------------------------------------
ONLY_BOARD=""
ONLY_EXAMPLE=""
VERBOSE=0
LIST_ONLY=0

while [ $# -gt 0 ]; do
  case "$1" in
    --board) ONLY_BOARD="$2"; shift 2 ;;
    --example) ONLY_EXAMPLE="$2"; shift 2 ;;
    -v|--verbose) VERBOSE=1; shift ;;
    --list) LIST_ONLY=1; shift ;;
    -h|--help)
      sed -n '2,20p' "$0"
      exit 0
      ;;
    *)
      echo "Unknown argument: $1" >&2
      exit 2
      ;;
  esac
done

if ! command -v arduino-cli >/dev/null 2>&1; then
  echo "error: arduino-cli not found on PATH" >&2
  exit 2
fi

# ---------------------------------------------------------------------------
# Enumerate examples (any directory under examples/ containing a .ino).
# ---------------------------------------------------------------------------
EXAMPLES=()
while IFS= read -r -d '' ino; do
  EXAMPLES+=("$(basename "$(dirname "$ino")")")
done < <(find "$EXAMPLES_DIR" -mindepth 2 -maxdepth 2 -name "*.ino" -print0 | sort -z)

if [ "${#EXAMPLES[@]}" -eq 0 ]; then
  echo "error: no examples found under $EXAMPLES_DIR" >&2
  exit 2
fi

if [ "$LIST_ONLY" -eq 1 ]; then
  echo "Boards:"
  for row in "${BOARDS[@]}"; do
    IFS='|' read -r key fqbn _ <<<"$row"
    echo "  $key -> $fqbn"
  done
  echo "Examples:"
  for ex in "${EXAMPLES[@]}"; do
    echo "  $ex"
  done
  exit 0
fi

# ---------------------------------------------------------------------------
# Compile loop
# ---------------------------------------------------------------------------
PASS=0
FAIL=0
SKIPPED=0
declare -a FAILED_COMBOS=()
declare -a WARNED_COMBOS=()

for row in "${BOARDS[@]}"; do
  IFS='|' read -r board_key fqbn extra_args <<<"$row"

  if [ -n "$ONLY_BOARD" ] && [ "$board_key" != "$ONLY_BOARD" ]; then
    continue
  fi

  for example in "${EXAMPLES[@]}"; do
    if [ -n "$ONLY_EXAMPLE" ] && [ "$example" != "$ONLY_EXAMPLE" ]; then
      continue
    fi

    combo="$board_key:$example"
    printf '%-10s %-28s ' "$board_key" "$example"

    if [ -n "${SKIP[$combo]+x}" ]; then
      echo "SKIP  (${SKIP[$combo]})"
      SKIPPED=$((SKIPPED + 1))
      continue
    fi

    log="$(mktemp)"
    # shellcheck disable=SC2086 # extra_args is intentionally word-split
    if timeout "$COMPILE_TIMEOUT" arduino-cli compile \
        --fqbn "$fqbn" \
        $extra_args \
        "$EXAMPLES_DIR/$example" >"$log" 2>&1; then
      if grep -qiE "warning:" "$log"; then
        echo "PASS  (with warnings)"
        WARNED_COMBOS+=("$combo")
        if [ "$VERBOSE" -eq 1 ]; then
          grep -iE "warning:" "$log"
        fi
      else
        echo "PASS"
      fi
      PASS=$((PASS + 1))
    else
      echo "FAIL"
      FAIL=$((FAIL + 1))
      FAILED_COMBOS+=("$combo")
      if [ "$VERBOSE" -eq 1 ]; then
        echo "----- $combo -----"
        cat "$log"
        echo "-------------------"
      fi
    fi
    rm -f "$log"
  done
done

# ---------------------------------------------------------------------------
# Summary
# ---------------------------------------------------------------------------
echo
echo "=========================================="
echo "PASS: $PASS   FAIL: $FAIL   SKIP: $SKIPPED"
if [ "${#WARNED_COMBOS[@]}" -gt 0 ]; then
  echo "Compiled with warnings:"
  for c in "${WARNED_COMBOS[@]}"; do
    echo "  - $c"
  done
fi
if [ "$FAIL" -gt 0 ]; then
  echo "Failed:"
  for c in "${FAILED_COMBOS[@]}"; do
    echo "  - $c"
  done
  echo "Re-run with -v for full compiler output, or:"
  echo "  ./test_compile.sh --board <board> --example <example> -v"
  exit 1
fi
exit 0
