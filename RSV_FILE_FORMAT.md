# Sony RSV File Format Documentation

This document describes the Sony RSV file format based on reverse engineering efforts during the development of RSV recovery functionality for untrunc.

## Overview

RSV files are incomplete or corrupted video files created by Sony cameras (e.g., Sony FX3) when recording is interrupted unexpectedly (power loss, card removal, etc.). The `.rsv` file extension denotes a reserved, backup, or temporary file that contains raw video data but lacks the proper container structure (like MP4 or MXF) needed for playback. Despite being incomplete, RSV files contain valid video and audio data; they simply lack the proper MP4 container structure (moov atom) needed for playback.

Unlike standard MP4 files which use interleaved chunks, RSV files use a **GOP-based block structure** where all components of a GOP (Group of Pictures) are stored contiguously.

## File Structure

### High-Level Layout

```
[GOP 0][GOP 1][GOP 2]...[GOP N][incomplete data]
```

Each GOP contains three sections in order:
1. **rtmd block** - Timed metadata (variable packet count, typically 12 packets)
2. **Video essence** - H.264/AVC or H.265/HEVC video frames (frame count detected from first GOP, assumed constant)
3. **Audio essence** - PCM audio samples (size calculated once from first GOP's frame count, reused for all GOPs)

### GOP Structure Detail

```
┌─────────────────────────────────────────────────────────────┐
│                         GOP N                               │
├─────────────────┬──────────────────────┬───────────────────-┤
│   rtmd Block    │    Video Frames      │   Audio Chunk      │
│   Variable size │    Variable size     │   Variable size    │
│   (N × packet)  │    (variable count)  │   (calculated)     │
└─────────────────┴──────────────────────┴───────────────────-┘
```

**Typical values** (for 12-frame GOP at 25fps):
- rtmd block: ~233,472 bytes (12 packets × 19,456 bytes)
- Video frames: ~5-6 MB (12 frames, variable size)
- Audio chunk: ~92,160 bytes (calculated from GOP duration)

## rtmd Block (Timed Metadata)

### Structure
- **Total size**: Variable (depends on packet count and packet size)
- **Packet count**: Usually 12 packets per GOP (can vary, observed 9-20)
- **Packet size**: Typically 19,456 bytes each (auto-detected from file)

### Packet Header Format

Each rtmd packet has a 12-byte header structure:

```
Offset:  0  1  2  3  4  5  6  7  8  9 10 11
Bytes:  00 1c 01 00 ?? ?? ?? ?? f0 01 00 10
        ├──────────┤ ├────────┤ ├─────────┤
         Prefix      Variable   Sony tag
         (constant)  metadata   (constant)
```

| Offset | Bytes | Description |
|--------|-------|-------------|
| 0-3 | `00 1c 01 00` | Constant prefix (identifies rtmd packet type) |
| 4-7 | variable | Camera metadata (timecode, frame counter, etc.) |
| 8-11 | `f0 01 00 10` | Sony-specific tag (constant across all packets) |

### Identifying Real rtmd Packets

**Important**: The 4-byte prefix `00 1c 01 00` can appear randomly in video/audio data as false positives. To reliably identify real rtmd packets, verify **both**:

1. Bytes 0-3 match `00 1c 01 00` (prefix)
2. Bytes 8-11 match `f0 01 00 10` (Sony tag)

**Observed header variants** (bytes 4-7 vary):
```
001c0100 230328e1 f0010010...  (most common)
001c0100 22f728ed f0010010...  (variant)
001c0100 230f28d5 f0010010...  (variant)
```

### Content
The rtmd data contains camera metadata such as:
- Timecode information
- Recording parameters  
- Camera settings
- GPS data (if enabled)

## Video Essence (H.264/AVC or H.265/HEVC)

### Supported Codecs
- **H.264/AVC** (codec name: `avc1`) - Most common
- **H.265/HEVC** (codec name: `hvc1`) - Also supported

### Frame Structure
- **Codec**: H.264/AVC High Profile or H.265/HEVC
- **NAL unit format**: Length-prefixed (4-byte big-endian length + NAL data)
- **Frames per GOP**: Auto-detected from the first GOP (typically 12), assumed constant throughout file

### Frame Identification

**H.264/AVC frames** begin with an **Access Unit Delimiter (AUD)** NAL:
```
00 00 00 02 09 XX
```
Where:
- `00 00 00 02` = NAL length (2 bytes)
- `09` = NAL type 9 (AUD)
- `XX` = AUD payload (typically `10` or `30`)

**H.265/HEVC frames** begin with an **Access Unit Delimiter (AUD)** NAL:
```
00 00 00 03 46 01 XX
```
Where:
- `00 00 00 03` = NAL length (3 bytes)
- `46` = First byte of HEVC NAL unit (NAL type 35 = AUD)
- `01 XX` = AUD payload

### NAL Unit Types Present
| Type | Name | Description |
|------|------|-------------|
| 9 | AUD | Access Unit Delimiter (frame start marker) |
| 6 | SEI | Supplemental Enhancement Information |
| 5 | IDR | Instantaneous Decoder Refresh (keyframe slice) |
| 1 | Non-IDR | Regular slice (P/B frame) |

### Frame Composition
A typical frame contains:
1. AUD NAL (2 bytes payload)
2. SEI NAL(s) (metadata)
3. Slice NAL(s) (actual video data)

**Keyframe (IDR) example:**
```
[AUD len=2][SEI len=19][SEI len=26][SEI len=29][SEI len=14][SEI len=5][IDR slice ×8]
```

**Inter-frame (P/B) example:**
```
[AUD len=2][SEI len=14][Non-IDR slice ×9]
```

### Important Note on NAL Parsing
The compressed slice data within NAL units can contain **any byte pattern**, including patterns that look like NAL length prefixes. Do NOT attempt to parse NAL units by reading length fields through slice data. Instead, search for AUD patterns to find frame boundaries.

### GOP Sizes
The implementation auto-detects `frames_per_gop` by counting AUD patterns in the first GOP (typically 12 frames). It then **assumes** this frame count is consistent throughout the entire file.

**Implementation behavior**:
- `frames_per_gop` is detected from the first GOP only
- `audio_chunk_size` is calculated **once** from this detected value and reused for all GOPs
- Frames are counted individually per GOP (would detect if sizes varied, but doesn't affect audio chunk calculation)
- The implementation assumes GOP sizes are constant - if they varied, audio chunk size calculation would be incorrect

**Note**: Analysis of one 50GB RSV file (8,655 GOPs) showed all GOPs had exactly 12 frames, but the implementation makes no guarantee about consistency. If GOP sizes varied, the code would need to calculate `audio_chunk_size` per-GOP based on the actual frame count.

## Audio Essence (PCM)

### Format
- **Codec**: PCM signed 16-bit big-endian (twos/pcm_s16be)
- **Sample rate**: 48,000 Hz
- **Channels**: 2 (stereo)
- **Bits per sample**: 16

### Chunk Structure
- **Chunk size**: Fixed per file (calculated once from first GOP's frame count)
- **Samples per chunk**: Calculated as `GOP_duration × sample_rate`
- **Bytes per sample**: Typically 4 (2 bytes × 2 channels for stereo 16-bit)
- **Duration per chunk**: Matches GOP duration (assumes constant GOP size)

### Calculation
Audio chunk size is calculated **once** based on the first GOP's frame count:
```
GOP_duration_sec = frames_per_gop / fps
samples_per_chunk = GOP_duration_sec × audio_sample_rate
chunk_size = samples_per_chunk × bytes_per_sample
```

**Example** (12 frames at 25fps, 48kHz stereo 16-bit):
```
GOP_duration = 12 frames ÷ 25 fps = 0.480 seconds
samples_per_chunk = 0.480s × 48000 Hz = 23,040 samples
chunk_size = 23,040 × 4 bytes = 92,160 bytes
```

**Important**: The implementation calculates `audio_chunk_size` from the first GOP's `frames_per_gop` and uses this fixed size for all GOPs. It **assumes** GOP sizes are constant throughout the file. If GOP sizes varied, this would cause incorrect audio chunk size calculation. The code counts frames per GOP individually but does not recalculate audio chunk size per GOP.

## Comparison with MP4

### MP4 (Interleaved)
```
[ftyp][moov][mdat: V₀ A₀ V₁ A₁ V₂ A₂ ...]
```
Video and audio chunks are interleaved within mdat.

### RSV (Block-based)
```
[rtmd₀ V₀₋₁₁ A₀][rtmd₁ V₁₂₋₂₃ A₁][rtmd₂ V₂₄₋₃₅ A₂]...
```
Each GOP's data is stored as a contiguous block.

## File Identification

### Magic Bytes
RSV files can be identified by checking offset 0 for the rtmd pattern. The implementation uses a specific header pattern for detection:

```
Offset 0-7:  00 1c 01 00 23 03 28 e1
```

This matches the most common rtmd header variant. For more robust detection, verify:
```
Offset 0-3:  00 1c 01 00  (prefix)
Offset 8-11: f0 01 00 10  (Sony tag)
```

Note: Bytes 4-7 are variable and should not be used for identification. The implementation auto-detects RSV files when this pattern is found at offset 0, or can be forced with the `-rsv` command-line flag.

### No Standard Container
RSV files lack:
- `ftyp` atom (file type)
- `moov` atom (movie metadata)
- Standard MP4 box structure

The entire file consists of raw GOP blocks.

## Recovery Process

To recover an RSV file using untrunc:

1. **Obtain reference file**: Need a properly-recorded MP4 from the same camera with matching codec parameters (H.264/AVC or H.265/HEVC).

2. **Run untrunc**:
   ```bash
   untrunc reference.mp4 corrupt.RSV
   ```
   Or explicitly enable RSV mode:
   ```bash
   untrunc -rsv reference.mp4 corrupt.RSV
   ```

3. **Auto-detection**:
   - RSV files are automatically detected if they start with the rtmd pattern at offset 0
   - The implementation auto-detects `rtmd_packet_size` by finding the distance between the first two rtmd patterns
   - The implementation auto-detects `frames_per_gop` by counting AUD patterns in the first GOP
   - Audio chunk size is calculated **once** from the first GOP's frame count and reused for all GOPs

4. **Parse GOP structure**:
   - Find rtmd packets by checking both prefix (`001c0100`) AND Sony tag (`f0010010` at offset 8)
   - Count consecutive rtmd packets (using auto-detected packet size) to find video start
   - Extract video frames by finding AUD patterns:
     - H.264: `00 00 00 02 09` (NAL type 9)
     - HEVC: `00 00 00 03 46` (NAL type 35)
   - Audio chunk follows immediately after video frames (size calculated from GOP duration)

5. **Build MP4 container**:
   - Use codec parameters from reference file
   - Create proper stbl atoms (stco, stsz, stsc, stts, stss, ctts)
   - Map frame offsets to the RSV data

6. **Handle GOP structure**: 
   - Count actual AUD patterns per GOP (frames are counted individually)
   - Count consecutive valid rtmd packets per GOP (rtmd count can vary)
   - Avoid false positive rtmd matches by verifying the Sony tag at offset 8
   - **Note**: Audio chunk size is calculated from the first GOP and reused for all GOPs. The implementation assumes GOP sizes are constant throughout the file - if they varied, this would be incorrect.

## Known Cameras

This format has been observed in:
- Sony FX3
- Other Sony cameras using XAVC codec

## Typical File Characteristics

| Property | Typical Value |
|----------|---------------|
| Video codec | H.264 High Profile or H.265/HEVC |
| Resolution | 3840×2160 (4K) |
| Frame rate | 25 fps (variable) |
| GOP size | Auto-detected from first GOP (typically 12), assumed constant |
| Audio codec | PCM S16BE (twos/sowt) |
| Audio rate | 48000 Hz (variable) |
| Bitrate | ~100 Mbps |
| rtmd packet size | 19,456 bytes (auto-detected) |

## References

- ITU-T H.264 / ISO/IEC 14496-10 (AVC)
- ISO/IEC 14496-12 (MP4 container)
- Sony XAVC format documentation

## Implementation Notes

### Auto-Detection Features

The untrunc implementation includes several auto-detection features:

1. **RSV File Detection**: Automatically detects RSV files by checking for rtmd pattern at offset 0
2. **rtmd Packet Size**: Auto-detects packet size by measuring distance between first two rtmd patterns
3. **Frames per GOP**: Auto-detects by counting AUD patterns in the first GOP
4. **Audio Chunk Size**: Calculated once from the first GOP's frame count and reused for all GOPs (assumes GOP sizes are constant throughout the file)
5. **Codec Support**: Supports both H.264/AVC (`avc1`) and H.265/HEVC (`hvc1`) codecs

### Command-Line Usage

```bash
# Auto-detect RSV file
untrunc reference.mp4 corrupt.RSV

# Force RSV mode
untrunc -rsv reference.mp4 corrupt.RSV
```

The `-rsv` flag forces RSV recovery mode even if auto-detection fails.

---

*This documentation was created during the development of RSV recovery support for untrunc. Last updated: November 2025*

