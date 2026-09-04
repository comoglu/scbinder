#!/usr/bin/env bash
#
# SPDX-License-Identifier: AGPL-3.0-or-later
# Copyright (C) 2026 Mustafa Çomoğlu
#
# Run the Earthworm side of the scbinder regression harness:
#   startstop (rings + binder_ew) -> sniffring capture -> pickinject ->
#   settle -> pau -> results in out/
#
# Usage: ./run_binder_ew.sh <picks_ew.txt>
#
set -euo pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"
PICKFILE="$(readlink -f "${1:?usage: run_binder_ew.sh <picks_ew.txt>}")"

export EW_HOME=/home/ubuntu/earthworm_git
export EW_VERSION=.
export EW_PARAMS="$HERE/params/"
export EW_LOG="$HERE/log/"
export EW_DATA_DIR="$HERE/out/"
export EW_INSTALLATION=INST_UNKNOWN
export PATH="$EW_HOME/bin:$PATH"

mkdir -p "$HERE/out" "$HERE/log"

# 1. Build pickinject if needed
if [ ! -x "$HERE/pickinject" ] || [ "$HERE/pickinject.c" -nt "$HERE/pickinject" ]; then
	gcc -o "$HERE/pickinject" "$HERE/pickinject.c" -D_LINUX -D_USE_POSIX_SIGNALS \
		-I"$EW_HOME/include" \
		"$EW_HOME/lib/transport.o" "$EW_HOME/lib/getutil.o" \
		"$EW_HOME/lib/kom.o" "$EW_HOME/lib/logit_mt.o" \
		"$EW_HOME/lib/time_ew.o" "$EW_HOME/lib/sleep_ew.o" \
		"$EW_HOME/lib/sema_ew.o" "$EW_HOME/lib/threads_ew.o" \
		-lpthread
	echo ">> pickinject built"
fi

# 2. Fresh event-id state
echo "next_id 1" > "$HERE/params/quake_id.d"

cleanup() {
	pau >/dev/null 2>&1 || true
	sleep 2
	pkill -f 'startstop startstop_unix.d' 2>/dev/null || true
	pkill -x startstop 2>/dev/null || true
	pkill -x binder_ew 2>/dev/null || true
	pkill -f 'sniffring PICK_RING' 2>/dev/null || true
}
trap cleanup EXIT

# 3. Start rings + binder_ew
cd "$HERE/params"
startstop > "$HERE/out/startstop.log" 2>&1 &
sleep 4

if ! pgrep -f binder_ew >/dev/null; then
	echo "ERROR: binder_ew did not start — see $HERE/out/startstop.log and $EW_LOG"
	exit 1
fi
echo ">> binder_ew running"

# 4. Capture everything binder emits on the ring
sniffring PICK_RING > "$HERE/out/ring_capture.log" 2>&1 &
sleep 1

# 5. Inject the picks
"$HERE/pickinject" PICK_RING "$PICKFILE"

# 6. Let binder finish associating, then shut down (trap does the pau)
sleep 10
echo ">> done — ring capture: $HERE/out/ring_capture.log"
echo ">> binder log:        $(ls -t "$EW_LOG" | head -1)"
