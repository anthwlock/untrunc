# Reference-driven MXF repair

## Workflow

    untrunc -mxf reference.mxf damaged.mxf
    untrunc -mxf -dst repaired.mxf reference.mxf damaged.mxf

MXF uses SMPTE KLV partition packs rather than MP4 atoms. The repair path therefore has its
own streaming parser and does not route MXF data through the MP4 Atom classes.

The reference must come from the same recorder profile. The implementation verifies the
Operational Pattern, BodySID, IndexSID, stream count, codec identifiers, raster dimensions,
sample formats, sample rates, time bases, and codec configuration before producing output.

## Reconstruction

1. Scan both files with fixed-size windows and 64-bit offsets. File size is not capped and
   recording contents are never loaded wholesale into memory.
2. Locate and bounds-check Header, Body, and Footer Partition Pack KLVs plus any existing
   Random Index Pack.
3. Copy only reference header metadata; reference essence is never copied.
4. Stream damaged-file body and footer data into a structural intermediate.
5. Rewrite ThisPartition, PreviousPartition, and FooterPartition references after the header
   size change and discard a stale RIP.
6. Open the reconstructed intermediate through libavformat and compare its complete stream
   profile against the known-good reference.
7. Demux every recoverable damaged-file packet and remux it through the FFmpeg MXF or
   MXF OP-Atom muxer. This rebuilds edit-unit indexing, durations, header/footer metadata,
   KAG layout, partition offsets, and the RIP from actual recovered packets.
8. Remove the structural intermediate. A failed validation or remux also removes partial
   output rather than leaving a file that appears repaired.

The final remux delegates codec and wrapping support to the FFmpeg version linked into
untrunc, so new MXF codecs supported by libavformat do not require parallel packet parsers
inside untrunc.

## Refusal rules

Repair fails without modifying either input when:

- no bounded Header or Body Partition Pack can be found;
- BER lengths or KLV extents exceed the source file;
- reference and damaged Operational Patterns, SIDs, or stream profiles differ;
- libavformat cannot demux the recovered essence;
- the selected MXF muxer cannot represent one of the recovered streams;
- no media packets can be recovered;
- writing or finalizing the new MXF fails.

There is intentionally no force option that bypasses reference compatibility.

## Tests

The regression suite generates real OP-1a MPEG-2 video plus PCM audio fixtures and verifies:

- a destroyed header is replaced from the reference;
- damaged-file video and audio packets are recovered and reindexed;
- the result is readable by ffprobe with the expected codecs and packet count;
- the reference recording is not substituted for damaged essence;
- temporary structural files are removed;
- Operational Pattern mismatches are rejected;
- same-OP files with incompatible raster descriptors are rejected.
