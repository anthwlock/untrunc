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
	int limit = stsd.contentSize() - 12 - 28;  // esds is 28 till 37 byte.
	const uchar* start = stsd.content(12, limit);
	char pattern[5] = "esds";
	int found = 0;
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
	size_t off = (start - stsd.content());
	size_t len = stsd.contentSize(off);
	logg(V, "found esds atom after: ", off, '\n');
	logg(V, "remaining content size: ", len, '\n');

	is_ok = decode(start);
}

bool AudioConfig::decode(const uchar* start) {
	// start += 4  +1 [+3] +4  +1 [+3] +1+1+1+3+4+4  +1 [+3] +1  +2 = 28 [+9]
	logg(V, "parsing esds atom.\n");
	int off = 0;
	start += 4;      // version + flag.

	uint tag = readBits(8, &start, &off);  // tag = 3.
	assert(tag == 3);
	if (start[0] == 0x80 && start[1] == 0x80 && start[2] == 0x80)
		start += 3;  // Skip extended start tag.
	start += 4;      // length(8-bit) es_id(16-bit) stream_priority(8-bit).

	tag = readBits(8, &start, &off);       // tag = 4.
	assert(tag == 4);
	if (start[0] == 0x80 && start[1] == 0x80 && start[2] == 0x80)
		start += 3;  // Skip extended start tag.
	start += 1;      // length(8-bit).
	object_type_id_ = readBits(8, &start, &off);
	if (object_type_id_ != 0x40)
		logg(V, "in esds - object_type_id_ != 0x40 (MPEG-4 audio)\n");
	stream_type_ = readBits(6, &start, &off);
	if (stream_type_ != 5)
		logg(W, "in esds - stream_type != 5 (audio)");
	readBits(2, &start, &off);  // upstream_flag(1-bit) reserved(1-bit)=1.
	buffer_size_ = readBits(24, &start, &off);
	max_bitrate_ = readBits(32, &start, &off);
	avg_bitrate_ = readBits(32, &start, &off);

	tag = readBits(8, &start, &off);       // tag = 5.
	assert(tag == 5);
	if (start[0] == 0x80 && start[1] == 0x80 && start[2] == 0x80)
		start += 3;  // Skip extended start tag.
	start += 1;      // length(8-bit).

	// Audio Specific Config.
	object_type_ = readBits(5, &start, &off);
	frequency_index_ = readBits(4, &start, &off);
	channel_config_ = readBits(4, &start, &off);
	frame_length_flag = readBits(1, &start, &off);
	readBits(1, &start, &off);  // dependsOnCoreCoder(1-bit).
	readBits(1, &start, &off);  // extensionFlag(1-bit).

	logg(V, "esds:\nobject_type: ", unsigned(object_type_), '\n');
	logg(V, "avg_bitrate: ", avg_bitrate_, '\n');
	logg(V, "max_bitrate: ", max_bitrate_, '\n');
	logg(V, "frame_length_flag: ", frame_length_flag, '\n');

	return true;
}


// vim:set ts=4 sw=4 sts=4 noet:
