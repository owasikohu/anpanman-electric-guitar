#!/usr/bin/env bash
set -euo pipefail

gain="${1:-4}"
output="${2:-pipewire}"
buffer_ms="${3:-80}"

if [[ ! "$gain" =~ ^([0-9]+([.][0-9]*)?|[.][0-9]+)$ ]]; then
    echo "usage: $0 [gain 0..10] [pipewire|direct] [buffer-ms]" >&2
    exit 2
fi

if [[ ! "$buffer_ms" =~ ^[0-9]+$ ]] || (( buffer_ms < 10 )); then
    echo "buffer-ms must be an integer of at least 10" >&2
    exit 2
fi

period_ms=$((buffer_ms / 8))
(( period_ms < 2 )) && period_ms=2
(( period_ms > 10 )) && period_ms=10
buffer_us=$((buffer_ms * 1000))
period_us=$((period_ms * 1000))
queue_ns=$((buffer_ms * 1000000))

case "$output" in
    pipewire)
        sink=(pipewiresink sync=true)
        ;;
    direct)
        # ThinkPad's analog playback device in the current ALSA card layout.
        sink=(alsasink device=plughw:1,0 buffer-time="$buffer_us" latency-time="$period_us" sync=true)
        ;;
    *)
        echo "output must be 'pipewire' or 'direct'" >&2
        exit 2
        ;;
esac

echo "Monitor: gain=${gain}x, output=${output}, buffer=${buffer_ms}ms, period=${period_ms}ms" >&2
echo "Stop with Ctrl+C. Keep speaker volume low to avoid feedback." >&2

gst-launch-1.0 -q \
    alsasrc device=hw:Microphone,0 buffer-time="$buffer_us" latency-time="$period_us" \
    ! audio/x-raw,format=S16LE,rate=48000,channels=1 \
    ! volume volume="$gain" \
    ! audioconvert \
    ! audioresample \
    ! queue max-size-buffers=0 max-size-bytes=0 max-size-time="$queue_ns" leaky=downstream \
    ! "${sink[@]}"
