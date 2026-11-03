#!/bin/sh
set -eu
FFMPEG=${FFMPEG:-ffmpeg}
FFPROBE=${FFPROBE:-ffprobe}
UNTRUNC=${UNTRUNC:-./untrunc}
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

make_mxf() {
 "$FFMPEG" -v error -y -f lavfi -i "$1=size=$4:rate=25"   -f lavfi -i sine=frequency="$2":sample_rate=48000 -t 2   -c:v mpeg2video -pix_fmt yuv422p -g 12 -b:v 2M   -c:a pcm_s16le -ar 48000 -f mxf "$3"
}
make_mxf testsrc 440 "$work/reference.mxf" 160x90
make_mxf testsrc 440 "$work/wrong-raster-reference.mxf" 320x180
make_mxf testsrc2 880 "$work/complete-damaged.mxf" 160x90

python3 - "$work" <<'PY'
import pathlib,sys
p=pathlib.Path(sys.argv[1])
d=bytearray((p/"complete-damaged.mxf").read_bytes())
body=bytes.fromhex("060e2b34020501010d0102010103")
off=d.index(body)
assert off>0
damaged=bytearray(d)
damaged[:off]=b"\0"*off
(p/"damaged.mxf").write_bytes(damaged)
incompatible=bytearray(damaged)
value=off+16
first=incompatible[value]
ber=1 if first<128 else 1+(first&0x7f)
incompatible[value+ber+64+15]^=1
(p/"incompatible.mxf").write_bytes(incompatible)
PY

"$UNTRUNC" -mxf -dst "$work/fixed.mxf" "$work/reference.mxf" "$work/damaged.mxf"
video=$("$FFPROBE" -v error -select_streams v:0 -show_entries stream=codec_name  -of default=noprint_wrappers=1:nokey=1 "$work/fixed.mxf")
audio=$("$FFPROBE" -v error -select_streams a:0 -show_entries stream=codec_name  -of default=noprint_wrappers=1:nokey=1 "$work/fixed.mxf")
packets=$("$FFPROBE" -v error -count_packets -show_entries stream=nb_read_packets  -of csv=p=0 "$work/fixed.mxf" | awk -F, '{for(i=1;i<=NF;i++)s+=$i} END{print s+0}')
test "$video" = mpeg2video
test "$audio" = pcm_s16le
test "$packets" -gt 50
cmp "$work/fixed.mxf" "$work/reference.mxf" >/dev/null && {
 echo "repair copied the reference recording instead of damaged essence" >&2; exit 1;
}
test ! -e "$work/fixed.mxf.untrunc-structure.tmp"

if "$UNTRUNC" -mxf -dst "$work/no.mxf" "$work/reference.mxf" "$work/incompatible.mxf"; then
 echo "incompatible Operational Pattern was accepted" >&2; exit 1
fi
test ! -e "$work/no.mxf"
if "$UNTRUNC" -mxf -dst "$work/wrong-profile.mxf" "$work/wrong-raster-reference.mxf" "$work/damaged.mxf"; then
 echo "incompatible stream profile was accepted" >&2; exit 1
fi
test ! -e "$work/wrong-profile.mxf"