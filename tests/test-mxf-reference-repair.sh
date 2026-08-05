#!/bin/sh
set -eu
UNTRUNC=${UNTRUNC:-./untrunc}
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM
python3 - "$work" <<'PY'
import pathlib,struct,sys
root=pathlib.Path(sys.argv[1]); prefix=bytes.fromhex("060e2b34020501010d01020101")
op=bytes.fromhex("060e2b34040101010d01020101010900"); rip=bytes.fromhex("060e2b34020501010d01020101110100")
def ber(n): return b"\x83"+n.to_bytes(3,"big")
def part(kind,this,marker=b"",pattern=op):
 v=struct.pack(">HHIQQQQQIQI16s",1,3,1,this,0,0,len(marker),0,0,0,1,pattern)+marker
 return prefix+bytes((kind,4,0))+ber(len(v))+v
gh=part(2,0,b"KNOWN-GOOD-HEADER"); gb=part(3,len(gh),b"REFERENCE-ESSENCE")
(root/"good.mxf").write_bytes(gh+gb)
bh=part(2,0,b"BROKEN-HEADER"); bb=part(3,len(bh),b"DAMAGED-FILE-ESSENCE")
(root/"damaged.mxf").write_bytes(bh+bb+rip+ber(16)+struct.pack(">IQI",1,len(bh),36))
badop=bytes.fromhex("060e2b34040101010d01020101010a00")
(root/"incompatible.mxf").write_bytes(bh+part(3,len(bh),b"ESSENCE",badop))
PY
"$UNTRUNC" -mxf -dst "$work/fixed.mxf" "$work/good.mxf" "$work/damaged.mxf"
grep -a -q "KNOWN-GOOD-HEADER" "$work/fixed.mxf"
grep -a -q "DAMAGED-FILE-ESSENCE" "$work/fixed.mxf"
! grep -a -q "REFERENCE-ESSENCE" "$work/fixed.mxf"
python3 - "$work/fixed.mxf" <<'PY'
import pathlib,sys
d=pathlib.Path(sys.argv[1]).read_bytes()
rip=bytes.fromhex("060e2b34020501010d01020101110100")
body=bytes.fromhex("060e2b34020501010d01020101030400")
assert d.count(rip)==1
off=d.index(body); value=off+20
assert int.from_bytes(d[value+8:value+16],"big")==off
assert int.from_bytes(d[value+16:value+24],"big")==0
PY
if "$UNTRUNC" -mxf -dst "$work/no.mxf" "$work/good.mxf" "$work/incompatible.mxf"; then
 echo "incompatible Operational Pattern was accepted" >&2; exit 1
fi
test ! -e "$work/no.mxf"