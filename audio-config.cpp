//==================================================================//
/*                                                                  *
	Untrunc - audio-configm.cpp

	Untrunc is GPL software; you can freely distribute,
	redistribute, modify & use under the terms of the GNU General
	Public License; either version 2 or its successor.

	Untrunc is distributed under the GPL "AS IS", without
	any warranty; without the implied warranty of merchantability
	or fitness for either an expressed or implied particular purpose.

	Please see the included GNU General Public License (GPL) for
	your rights and further details; see the file COPYING. If you
	cannot, write to the Free Software Foundation, 59 Temple Place
	Suite 330, Boston, MA 02111-1307, USA.  Or www.fsf.org

	Copyright 2010 Federico Ponchio
	with contributions from others.
 *                                                                  */
//==================================================================//

#include "audio-config.h"

#include <cassert>


AudioConfig::AudioConfig(const Atom& stsd) {
	// Find esds atom.
	const uchar* start = stsd.content_.data() + 12;
	char pattern[5] = "esds";
	int found = 0;
	int limit = stsd.length_ - 16;
	while (limit--) {
		if (*start++ == pattern[found])
			found++;
		else if (found)
			found = 0;
		if (found == 4)
			break;
	}
	if (found != 4) {
		logg(V, "esds signature not found\n");
		is_ok = false;
		return;
	}
	int off = start - stsd.content_.data();
	int len = stsd.length_ - off;
	logg(V, "found avcC after: ", off, '\n');
	logg(V, "remaining len:", len, '\n');

	is_ok = decode(start);
}

bool AudioConfig::decode(const uchar* start) {
	logg(V, "parsing esds ...\n");
	int off = 0;
	start += 4;                            // version + flag.
	uint tag = readBits(8, &start, &off);  // 3
	if (start[0] == 0x80 && start[1] == 0x80 && start[2] == 0x80)
		start += 3;  // Skip extended start tag.
	assert(tag == 3);
	start += 4;      // ES descriptor: length(8) es_id(16) stream_priority(8).

	tag = readBits(8, &start, &off);  // 4
	assert(tag == 4);
	if (start[0] == 0x80 && start[1] == 0x80 && start[2] == 0x80)
		start += 3;  // Skip extended start tag.
	start += 1;      // length(8).
	object_type_id_ = readBits(8, &start, &off);
	if (object_type_id_ != 0x40)
		logg(V, "in esds - object_type_id_ != 0x40 (MPEG-4 audio)\n");
	stream_type_ = readBits(6, &start, &off);
	if (stream_type_ != 5)
		logg(W, "in esds - stream_type != 5 (audio)");
	readBits(2, &start, &off);  // upstream_flag(1) reserved(1)=1.
	buffer_size_ = readBits(24, &start, &off);
	max_bitrate_ = readBits(32, &start, &off);
	avg_bitrate_ = readBits(32, &start, &off);

	tag = readBits(8, &start, &off);  // 4
	assert(tag == 5);
	if (start[0] == 0x80 && start[1] == 0x80 && start[2] == 0x80)
		start += 3;  // Skip extended start tag.
	start += 1;      // lenght(8).

	// Audio Specific Config.

	object_type_ = readBits(5, &start, &off);
	frequency_index_ = readBits(4, &start, &off);
	channel_config_ = readBits(4, &start, &off);
	frame_length_flag = readBits(1, &start, &off);
	readBits(1, &start, &off);  // dependsOnCoreCoder.
	readBits(1, &start, &off);  // extensionFlag.

	logg(V, "esds:\n object_type: ", object_type_, '\n');
	logg(V, "avg_bitrate: ", avg_bitrate_, '\n');
	logg(V, "max_bitrate: ", max_bitrate_, '\n');
	logg(V, "frame_length_flag: ", frame_length_flag);

	return true;
}


// vim:set ts=4 sw=4 sts=4 noet:
