#include "rsv.h"
#include "mp4.h"
#include "common.h"

#include <cstring>

using namespace std;


bool isRtmdHeader(const uchar* buff) {
	// Real rtmd packets have:
	//   - Bytes 0-3: 001c0100 (constant prefix)
	//   - Bytes 4-7: variable (camera metadata like timestamp)
	//   - Bytes 8-11: f0010010 (constant Sony tag)
	// False positives (random video data) match bytes 0-3 but NOT bytes 8-11

	// Check constant prefix (bytes 0-3): 00 1c 01 00
	if (buff[0] != 0x00 || buff[1] != 0x1c || buff[2] != 0x01 || buff[3] != 0x00)
		return false;

	// Check Sony tag (bytes 8-11): f0 01 00 10
	if (buff[8] != 0xf0 || buff[9] != 0x01 || buff[10] != 0x00 || buff[11] != 0x10)
		return false;

	return true;
}

bool isPointingAtRtmdHeader(FileRead& file) {
	if (file.atEnd()) return false;
	return isRtmdHeader(file.getPtr(12));
}

// RSV file repair (Ben's method) - processes GOP-based structure
// For more details see:
// https://github.com/anthwlock/untrunc/pull/254
// https://github.com/anthwlock/untrunc/blob/rsv-ben/RSV_FILE_FORMAT.md
void Mp4::repairRsvBen(const string& filename) {
	logg(I, "using RSV Ben recovery mode\n");

	auto& file_read = openFile(filename);

	// Set up current_mdat_ for the RSV file as if it were an mdat starting at offset 0
	delete current_mdat_;
	current_mdat_ = new BufferedAtom(file_read);
	current_mdat_->name_ = "mdat";
	current_mdat_->start_ = -8;  // Pretend mdat header is at -8 so content starts at 0
	current_mdat_->file_end_ = file_read.length();

	// RSV structure parameters - all auto-detected from RSV file itself
	// - rtmd_packet_size: distance between consecutive rtmd patterns (fallback: 19456)
	// - frames_per_gop: count of video frames in first GOP (fallback: 12)
	int rtmd_packet_size = 19456;
	int frames_per_gop = 12;

	// AUD (Access Unit Delimiter) patterns with length prefix
	// H.264: NAL type 9 (0x09), typically 2-byte payload: 00 00 00 02 09 XX
	// HEVC:  NAL type 35 (0x23), in 2-byte header: 00 00 00 03 46 01 XX
	//        HEVC NAL type = (first_byte >> 1) & 0x3F = (0x46 >> 1) & 0x3F = 35
	const uchar aud_pattern_h264[] = {0x00, 0x00, 0x00, 0x02, 0x09};
	const uchar aud_pattern_hevc[] = {0x00, 0x00, 0x00, 0x03, 0x46};
	// aud_pattern will be set after we determine video codec type

	// Find video and audio track indices BEFORE clearing tracks
	// (clear() wipes out the keyframes_ we need)
	// Support both H.264 (avc1) and H.265/HEVC (hvc1)
	int video_track_idx = getTrackIdx2("avc1");
	bool is_hevc = false;
	if (video_track_idx < 0) {
		video_track_idx = getTrackIdx2("hvc1");
		is_hevc = true;
	}
	// Find all audio tracks (some cameras have multiple mono tracks)
	vector<int> audio_track_indices;
	string audio_codec_name;
	for (uint i = 0; i < tracks_.size(); i++) {
		const string& codec = tracks_[i].codec_.name_;
		if (codec == "twos" || codec == "sowt" || codec == "ipcm") {
			audio_track_indices.push_back(i);
			if (audio_codec_name.empty()) audio_codec_name = codec;
		}
	}
	int audio_track_idx = audio_track_indices.empty() ? -1 : audio_track_indices[0];
	int num_audio_tracks = audio_track_indices.size();

	// Find rtmd track
	int rtmd_track_idx = getTrackIdx2("rtmd");

	if (video_track_idx < 0) {
		logg(ET, "no video track (avc1/hvc1) found in reference file\n");
		return;
	}
	logg(I, "video track: ", video_track_idx, " (", (is_hevc ? "HEVC" : "H.264"), "), audio tracks: ", num_audio_tracks, ", rtmd track: ", rtmd_track_idx, "\n");

	// Select AUD pattern based on codec
	const uchar* aud_pattern = is_hevc ? aud_pattern_hevc : aud_pattern_h264;

	// Get audio and video parameters from reference file BEFORE clearing
	int audio_sample_size = 4;  // default: stereo 16-bit = 4 bytes per sample
	int audio_sample_rate = 48000;  // default
	int video_timescale = 25000;    // default (for 25fps)
	int video_duration_per_sample = 1000;  // default (25fps = 25000/1000)

	if (video_track_idx >= 0) {
		Track& video_track = tracks_[video_track_idx];
		video_timescale = video_track.timescale_;
		if (!video_track.times_.empty()) {
			video_duration_per_sample = video_track.times_[0];
		}
		logg(V, "video timescale: ", video_timescale, ", duration per sample: ", video_duration_per_sample, "\n");

		// Note: frames_per_gop will be auto-detected from RSV file itself
		// (reference file may have different GOP settings than the RSV being recovered)
	}

	if (audio_track_idx >= 0) {
		Track& audio_track = tracks_[audio_track_idx];
		if (audio_track.constant_size_ > 0) {
			audio_sample_size = audio_track.constant_size_;
		}
		audio_sample_rate = audio_track.timescale_;  // For audio, timescale is sample rate
		logg(V, "audio sample size: ", audio_sample_size, ", sample rate: ", audio_sample_rate, ", tracks: ", num_audio_tracks, "\n");
	}

	// NOW clear tracks to prepare for RSV parsing
	duration_ = 0;
	for (uint i = 0; i < tracks_.size(); i++)
		tracks_[i].clear();

	// Get file size
	off_t file_size = file_read.length();
	logg(I, "RSV file size: ", file_size, " bytes\n");

	if (file_size > (1LL<<32)) {
		broken_is_64_ = true;
		logg(I, "using 64-bit offsets for the RSV file\n");
	}

	// Auto-detect RSV structure parameters from the file itself
	{
		vector<uchar> detect_buf(128 * 1024 * 1024);  // 128MB buffer for detection (GOPs can be 25MB+ at high bitrates)
		file_read.seek(0);
		size_t detect_read = min((off_t)detect_buf.size(), file_size);
		file_read.readChar((char*)detect_buf.data(), detect_read);

		// First, detect rtmd_packet_size by finding distance between first two rtmd patterns
		off_t first_rtmd = -1, second_rtmd = -1;
		for (size_t i = 0; i < min(detect_read, (size_t)100000) - 12; i++) {
			if (isRtmdHeader(detect_buf.data() + i)) {
				if (first_rtmd < 0) {
					first_rtmd = i;
				} else {
					second_rtmd = i;
					break;
				}
			}
		}

		if (first_rtmd >= 0 && second_rtmd > first_rtmd) {
			int detected_size = second_rtmd - first_rtmd;
			if (detected_size > 1000 && detected_size < 100000) {  // Sanity check
				rtmd_packet_size = detected_size;
				logg(I, "rtmd packet size auto-detected from RSV: ", rtmd_packet_size, " bytes\n");
			}
		} else {
			logg(W, "could not detect rtmd packet size, using default: ", rtmd_packet_size, "\n");
		}

		// Now detect frames_per_gop by counting frames in first GOP
		// Find first rtmd block end (count rtmd packets)
		int first_rtmd_count = 0;
		off_t detect_pos = 0;
		while (detect_pos + 12 < (off_t)detect_read && first_rtmd_count < 100) {
			if (isRtmdHeader(detect_buf.data() + detect_pos)) {
				first_rtmd_count++;
				detect_pos += rtmd_packet_size;
			} else {
				break;
			}
		}

		if (first_rtmd_count > 0) {
			off_t video_start = first_rtmd_count * rtmd_packet_size;

			// Find next rtmd block
			off_t next_rtmd = 0;
			for (size_t i = video_start + 500 * 1024; i < detect_read - 12; i++) {
				if (isRtmdHeader(detect_buf.data() + i)) {
					next_rtmd = i;
					break;
				}
			}

			if (next_rtmd > 0) {
				// Count AUD patterns between video_start and next_rtmd - audio_chunk_size margin
				// Use a generous margin for audio
				off_t video_end_approx = next_rtmd - 100000;  // 100KB margin
				int aud_count = 0;
				const uchar* aud_p = is_hevc ? aud_pattern_hevc : aud_pattern_h264;

				for (size_t i = video_start; i < (size_t)video_end_approx - 5; i++) {
					if (memcmp(detect_buf.data() + i, aud_p, 5) == 0) {
						aud_count++;
					}
				}

				if (aud_count >= 6 && aud_count <= 60) {  // Sanity check
					frames_per_gop = aud_count;
					logg(I, "frames per GOP auto-detected from RSV: ", frames_per_gop, "\n");
				}
			}
		}
	}

	// Calculate audio chunk size based on GOP duration
	// GOP duration = frames_per_gop * duration_per_sample / timescale
	// Audio samples per chunk = GOP duration * audio_sample_rate
	double fps = (double)video_timescale / video_duration_per_sample;
	double gop_duration_sec = (double)frames_per_gop / fps;
	int audio_samples_per_chunk = (int)(gop_duration_sec * audio_sample_rate);
	int audio_chunk_size_per_track = audio_samples_per_chunk * audio_sample_size;
	// Total audio size in RSV is for all tracks combined
	int total_audio_chunk_size = audio_chunk_size_per_track * max(1, num_audio_tracks);

	logg(I, "derived parameters: fps=", fps, ", GOP duration=", gop_duration_sec, "s, audio chunk=", total_audio_chunk_size, " bytes (", num_audio_tracks, " tracks)\n");

	// Process file GOP by GOP
	off_t pos = 0;
	int gop_count = 0;
	int total_video_frames = 0;
	int total_audio_chunks = 0;
	int total_rtmd_packets = 0;

	// Buffer for reading
	const size_t buf_size = 128 * 1024 * 1024;  // 128MB buffer (GOPs can be 25MB+ at high bitrates)
	vector<uchar> buffer(buf_size);

	while (pos < file_size) {
		// First, count rtmd packets to skip past them
		// Check using isRtmdHeader to avoid false positives
		int rtmd_count = 0;
		uchar rtmd_header[12];
		while (pos + rtmd_count * rtmd_packet_size < file_size && rtmd_count < 100) {
			file_read.seek(pos + rtmd_count * rtmd_packet_size);
			file_read.readChar((char*)rtmd_header, 12);
			if (isRtmdHeader(rtmd_header)) {
				rtmd_count++;
			} else {
				break;
			}
		}

		if (rtmd_count == 0) {
			logg(V, "no rtmd packets found at ", pos, ", ending\n");
			break;
		}

		off_t rtmd_end = pos + rtmd_count * rtmd_packet_size;
		off_t video_start = rtmd_end;  // Video starts immediately after rtmd block

		logg(V, "GOP ", gop_count, ": rtmd at ", pos, " (", rtmd_count, " packets), video starts at ", video_start, "\n");

		// Read buffer starting from video_start for frame detection
		file_read.seek(video_start);
		size_t to_read = min((off_t)buf_size, file_size - video_start);
		file_read.readChar((char*)buffer.data(), to_read);

		// Find all video frames by searching for AUD patterns
		// Use memchr for fast first-byte search, then verify remaining bytes
		vector<off_t> frame_offsets;
		vector<uint> frame_sizes;

		// Fast AUD pattern search using memchr
		// H.264 AUD: 00 00 00 02 09
		// HEVC AUD:  00 00 00 03 46
		const uchar* buf_ptr = buffer.data();
		const uchar* buf_end = buf_ptr + to_read - 5;
		const uchar* p = buf_ptr;
		const int aud_pattern_len = 5;

		while (p < buf_end) {
			// Use memchr to quickly find potential start (first 0x00)
			p = (const uchar*)memchr(p, 0x00, buf_end - p);
			if (!p) break;

			// Check against the codec-appropriate AUD pattern
			if (p + aud_pattern_len <= buf_end && memcmp(p, aud_pattern, aud_pattern_len) == 0) {
				frame_offsets.push_back(video_start + (p - buf_ptr));
				p += aud_pattern_len;
			} else {
				p++;
			}
		}

		// Find next rtmd block to determine where video region ends
		// Use memchr for fast search, then verify with isRtmdHeader
		off_t next_rtmd_start = file_size;
		size_t min_search_offset = min((size_t)(500 * 1024), to_read / 4);  // At least 500KB in

		p = buf_ptr + min_search_offset;
		buf_end = buf_ptr + to_read - 12;

		while (p < buf_end) {
			// Fast search for rtmd prefix first byte (0x00)
			p = (const uchar*)memchr(p, 0x00, buf_end - p);
			if (!p) break;

			// Verify using isRtmdHeader
			if (isRtmdHeader(p)) {
				next_rtmd_start = video_start + (p - buf_ptr);
				break;
			}
			p++;
		}

		// Audio region (all tracks combined) is before next rtmd
		off_t audio_boundary = next_rtmd_start - total_audio_chunk_size;

		// Filter out any frame offsets that are past the audio boundary
		while (!frame_offsets.empty() && frame_offsets.back() >= audio_boundary) {
			frame_offsets.pop_back();
		}

		// Calculate frame sizes
		for (size_t i = 0; i < frame_offsets.size(); i++) {
			if (i + 1 < frame_offsets.size()) {
				frame_sizes.push_back(frame_offsets[i + 1] - frame_offsets[i]);
			} else {
				// Last frame ends at audio boundary
				frame_sizes.push_back(audio_boundary - frame_offsets[i]);
			}
		}

		// Calculate where video ends (after last frame)
		off_t video_end = video_start;
		if (!frame_offsets.empty() && !frame_sizes.empty()) {
			video_end = frame_offsets.back() + frame_sizes.back();
		}

		logg(V, "GOP ", gop_count, ": found ", frame_offsets.size(), " frames, video ends at ", video_end, "\n");

		// Audio starts after video (at audio_boundary calculated earlier)
		off_t audio_start = audio_boundary;

		if (frame_offsets.empty()) {
			logg(V, "no more video frames found, ending\n");
			break;
		}

		// Process video frames
		int frames_in_gop = frame_offsets.size();

		// Add frames to track
		for (int f = 0; f < frames_in_gop; f++) {
			off_t frame_off = frame_offsets[f];
			uint frame_sz = frame_sizes[f];

			// Determine if keyframe (first frame of GOP is IDR)
			bool is_keyframe = (f == 0);

			Track& video_track = tracks_[video_track_idx];
			video_track.num_samples_++;
			if (is_keyframe) {
				video_track.keyframes_.push_back(video_track.sizes_.size());
			}
			video_track.sizes_.push_back(frame_sz);

			// Add chunk info - offsets are relative to file start (mdat content start = 0)
			Track::Chunk chunk;
			chunk.off_ = frame_off;  // Will be adjusted in saveVideo
			chunk.size_ = frame_sz;
			chunk.n_samples_ = 1;
			video_track.chunks_.push_back(chunk);

			total_video_frames++;
			pkt_idx_++;
		}

		logg(V, "GOP ", gop_count, ": ", frames_in_gop, " video frames, audio at ", audio_start, "\n");

		// Process audio chunk(s) if audio track(s) exist
		if (audio_track_idx >= 0 && audio_start + total_audio_chunk_size <= file_size) {
			// Each audio chunk contains multiple PCM samples
			int samples_in_chunk = audio_chunk_size_per_track / audio_sample_size;

			// Process each audio track (for multi-track setups like 4 mono channels)
			for (int t = 0; t < num_audio_tracks; t++) {
				Track& audio_track = tracks_[audio_track_indices[t]];
				audio_track.num_samples_ += samples_in_chunk;

				// Add chunk - each track gets its portion of the audio data
				// Audio data in RSV appears to be sequential per track
				Track::Chunk audio_chunk;
				audio_chunk.off_ = audio_start + t * audio_chunk_size_per_track;
				audio_chunk.size_ = audio_chunk_size_per_track;
				audio_chunk.n_samples_ = samples_in_chunk;
				audio_track.chunks_.push_back(audio_chunk);

				pkt_idx_ += samples_in_chunk;
			}

			total_audio_chunks++;

			// Next GOP starts after all audio tracks
			pos = audio_start + total_audio_chunk_size;
		} else {
			// No audio - still need to advance to next GOP
			pos = next_rtmd_start;
		}

		// Add rtmd packets to rtmd track if it exists
		// rtmd packets are at the start of each GOP, starting at 'pos' after audio processing
		// We need to use the rtmd position from THIS GOP (before video_start)
		if (rtmd_track_idx >= 0 && rtmd_count > 0) {
			Track& rtmd_track = tracks_[rtmd_track_idx];
			// Each GOP has rtmd_count packets, each of size rtmd_packet_size
			// The rtmd block for this GOP started at the old 'pos' value (before we updated it)
			// That was: pos_before_audio = (current pos) - total_audio_chunk_size - video_size - rtmd_size
			// But we saved it: rtmd starts at (video_start - rtmd_count * rtmd_packet_size)
			off_t rtmd_block_start = video_start - rtmd_count * rtmd_packet_size;

			// Add as a single chunk containing all rtmd packets for this GOP
			Track::Chunk rtmd_chunk;
			rtmd_chunk.off_ = rtmd_block_start;
			rtmd_chunk.size_ = rtmd_count * rtmd_packet_size;
			rtmd_chunk.n_samples_ = rtmd_count;
			rtmd_track.chunks_.push_back(rtmd_chunk);
			rtmd_track.num_samples_ += rtmd_count;

			total_rtmd_packets += rtmd_count;
			pkt_idx_ += rtmd_count;
		}

		gop_count++;

		// Progress indicator
		if (gop_count % 10 == 0) {
			outProgress(pos, file_size);
		}
	}

	logg(I, "RSV recovery complete:\n");
	logg(I, "  GOPs processed: ", gop_count, "\n");
	logg(I, "  Video frames: ", total_video_frames, "\n");
	logg(I, "  Audio chunks: ", total_audio_chunks, "\n");
	logg(I, "  RTMD packets: ", total_rtmd_packets, "\n");

	// Fix track timing
	for (auto& track : tracks_) track.fixTimes();

	// Save the recovered video
	auto filename_fixed = getPathRepaired(filename_ok_, filename);
	saveVideo(filename_fixed);
}