#!/usr/bin/env bash
set -euo pipefail

gain="${1:-4}"
playback_device="${2:-default}"

if [[ ! "$gain" =~ ^([0-9]+([.][0-9]*)?|[.][0-9]+)$ ]]; then
    echo "usage: $0 [gain] [playback-device]" >&2
    echo "example: $0 4 default" >&2
    exit 2
fi

echo "Pico microphone monitor: gain=${gain}x, playback=${playback_device}" >&2
echo "Stop with Ctrl+C. Keep the speaker volume low to avoid feedback." >&2

arecord \
    -D hw:Microphone,0 \
    -t raw -f S16_LE -r 48000 -c 1 \
    --buffer-time=500000 --period-time=10000 \
| ffmpeg \
    -hide_banner -loglevel warning \
    -f s16le -ar 48000 -ac 1 -i pipe:0 \
    -af "volume=${gain},alimiter=limit=0.95" \
    -f s16le -ar 48000 -ac 1 pipe:1 \
| aplay \
    -D "$playback_device" \
    -t raw -f S16_LE -r 48000 -c 1 \
    --buffer-time=500000 --period-time=10000
