#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
OUT="${1:-$ROOT/release/desk-mate-voice-$(date +%Y%m%d-%H%M%S)}"
BUILD_DIR="${BUILD_DIR:-$ROOT/build/firmware-feedback}"
BIN="$BUILD_DIR/desk-mate-app.ino.bin"
test -f "$BIN" || { echo "missing $BIN; build first" >&2; exit 1; }
mkdir -p "$OUT/firmware" "$OUT/mac" "$OUT/docs" "$OUT/evidence" "$OUT/tools"
cp "$BIN" "$OUT/firmware/"
for f in bootloader.bin partitions.bin boot_app0.bin flash_args; do
  test -f "$BUILD_DIR/$f" && cp "$BUILD_DIR/$f" "$OUT/firmware/" || true
done
cp -R "$ROOT/firmware/desk-mate-app" "$OUT/firmware/desk-mate-app"
rm -rf "$OUT/firmware/desk-mate-app/build" "$OUT/firmware/desk-mate-app/.pio" 2>/dev/null || true
cp -R "$ROOT/mac/src" "$OUT/mac/src"
cp -R "$ROOT/mac/tests" "$OUT/mac/tests"
cp -R "$ROOT/tools/lv_sim" "$OUT/tools/lv_sim"
cp -R "$ROOT/firmware/test" "$OUT/firmware/test"
cp -R "$ROOT/mac/scripts" "$OUT/mac/scripts"
cp -R "$ROOT/mac/bin" "$OUT/mac/bin"
cp "$ROOT/mac/requirements.lock" "$OUT/mac/" 2>/dev/null || true
cp "$ROOT/run.sh" "$ROOT/bootstrap.sh" "$ROOT/install-launch-agent.sh" "$ROOT/install-training-reminder-agent.sh" "$OUT/"
cp -R "$ROOT/mac/prompts" "$OUT/mac/prompts" 2>/dev/null || true
cp "$ROOT/feature.patch" "$OUT/" 2>/dev/null || true
cp "$ROOT/docs/DELIVERY_REPORT.md" "$ROOT/docs/INSTALL.md" "$ROOT/docs/OPERATIONS.md" \
   "$ROOT/docs/RECOVERY.md" "$ROOT/docs/MERGE_BACK.md" "$ROOT/docs/HARDWARE_ACCEPTANCE.md" \
   "$ROOT/docs/ROADMAP_VOICE_AND_GENERIC_CONTROL.md" "$ROOT/docs/FEEDBACK.md" "$OUT/docs/" 2>/dev/null || true
cp "$ROOT/tmp_half_done.md" "$OUT/" 2>/dev/null || true
cp "$ROOT/docs/evidence/"* "$OUT/evidence/" 2>/dev/null || true
cp "$ROOT/README.md" "$ROOT/MASTER_SPEC.md" "$OUT/" 2>/dev/null || true
( cd "$OUT" && find . -type f -not -path './.DS_Store' -print0 | xargs -0 shasum -a 256 > SHA256SUMS )
tar -C "$(dirname "$OUT")" -czf "$OUT.tar.gz" "$(basename "$OUT")"
echo "$OUT.tar.gz"
