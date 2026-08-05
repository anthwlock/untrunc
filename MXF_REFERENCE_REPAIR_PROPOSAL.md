# Proposal: reference-driven MXF repair

## Motivation

Untrunc currently repairs ISO Base Media File Format files by borrowing structure from a
known-good recording. MXF uses SMPTE KLV partition packs rather than MP4 atoms, but the same
operator workflow is useful for interrupted camera and broadcast recordings:

    untrunc -mxf reference.mxf damaged.mxf

The reference must come from the same recorder profile and use the same Operational Pattern,
essence container labels, track layout, edit rate, and descriptor configuration.

## Safe initial scope

The first implementation should target partitioned OP-1a files whose essence body remains
present but whose header metadata, footer partition, index table, or Random Index Pack is
missing or truncated.

1. Scan both files for SMPTE partition-pack Universal Labels and parse BER lengths.
2. Compare the Preface OperationalPattern, EssenceContainers set, Material/Source Package
   track layout, File Descriptors, edit rates, BodySID, and IndexSID. Refuse repair on any
   incompatibility unless a future explicit force option is supplied.
3. Copy structural header metadata from the reference, but never copy reference essence.
4. Walk recoverable KLV essence in the damaged body and derive actual edit-unit boundaries.
5. Rebuild IndexTableSegment entries from the damaged essence. Do not reuse reference
   stream offsets or durations.
6. Rewrite ThisPartition, PreviousPartition, FooterPartition, HeaderByteCount,
   IndexByteCount, BodyOffset, and KAG-aligned fill.
7. Generate a closed-complete footer partition and a new Random Index Pack.
8. Write damaged.mxf_fixed.mxf atomically and report preserved essence/edit-unit counts.

## Refusal and validation rules

Repair should fail without modifying the input when:

- no valid Header or Body Partition Pack can be found;
- reference and damaged Operational Patterns differ;
- essence container labels, track count, descriptors, edit rates, BodySID, or IndexSID differ;
- encrypted essence or an unsupported clip-wrapped/container mapping is encountered;
- a BER length, KLV extent, or partition offset exceeds the file;
- the damaged essence cannot be segmented without guessing.

The implementation should stream files and keep only metadata/index state in memory so
multi-hundred-gigabyte broadcast MXF files remain practical.

## Suggested architecture

Add an MXF-specific module instead of extending the MP4 Atom classes:

- MxfKlvReader: bounded UL/BER/KLV parsing and resynchronization;
- MxfPartitionMap: Header/Body/Footer/RIP discovery and offset validation;
- MxfReferenceProfile: compatibility fingerprint derived from metadata sets;
- MxfEssenceScanner: codec/container-specific edit-unit discovery;
- MxfWriter: KAG alignment, partition rewriting, index construction, and RIP output.

Start with frame-wrapped MPEG-2/XDCAM OP-1a, then add AVC-Intra and DNxHD/DNxHR fixtures.
Clip-wrapped formats should remain explicitly unsupported until their duration/index rules
are implemented.

## Tests

Use tiny generated or synthetic fixtures and assert:

- reference metadata is present in the repaired file;
- damaged-file essence is byte-identical and reference essence never leaks into output;
- partition chains and RIP offsets point inside the output and are monotonically valid;
- truncated Header, Footer, IndexTableSegment, and RIP cases repair independently;
- incompatible Operational Pattern, descriptor, edit-rate, and SID fixtures are rejected;
- malformed BER lengths and forged huge offsets are rejected without excessive allocation;
- ffprobe, mediainfo, and an MXF conformance checker can read the repaired duration,
  streams, timecode, and edit-unit count.

## Review strategy

Land the bounded KLV reader and partition-map tests first. Add the compatibility fingerprint
second, and add one essence scanner plus writer in a third change. This keeps parser security,
format compatibility, and media reconstruction independently reviewable.