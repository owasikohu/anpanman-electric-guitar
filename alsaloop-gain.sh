#!/usr/bin/env bash
set -euo pipefail

gain_db="${1:-12}"
latency_us="${2:-500000}"
script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
export ALSA_CONFIG_PATH="${script_dir}/pico-softvol.conf"

if [[ ! "$gain_db" =~ ^[0-9]+([.][0-9]+)?$ ]] || [[ ! "$latency_us" =~ ^[0-9]+$ ]]; then
    echo "usage: $0 [gain-dB] [latency-us]" >&2
    echo "example: $0 12 500000  # 12 dB is approximately 4x" >&2
    exit 2
fi

alsaloop \
    -C pico_gain -P default \
    -r 48000 -c 1 -f S16_LE \
    -t "$latency_us" -S 5 &
loop_pid=$!
trap 'kill "$loop_pid" 2>/dev/null || true' EXIT INT TERM

# The softvol control is created when pico_gain is opened.
for _ in 1 2 3 4 5; do
    if amixer -c Microphone sset 'Pico Capture Gain' "${gain_db}dB" >/dev/null 2>&1; then
        echo "Pico capture gain: ${gain_db} dB" >&2
        wait "$loop_pid"
        exit $?
    fi
    sleep 0.1
done

echo "Could not create the Pico Capture Gain control." >&2
kill "$loop_pid" 2>/dev/null || true
exit 1
