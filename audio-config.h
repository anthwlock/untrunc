//==================================================================//
/*                                                                  *
	Untrunc - audio-config.h

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

#ifndef AUDIO_CONFIG_H_
#define AUDIO_CONFIG_H_

#include "atom.h"
#include "common.h"


// Audio Object types.
enum {
	AAC_MAIN = 1,
	AAC_LC   = 2,  // Low Complexity
	AAC_SSR  = 3,  // Scaleable Sampe Rate
	AAC_LTP  = 4,  // Long Term Prediction
	// ...
	// see: https://wiki.multimedia.cx/index.php/MPEG-4_Audio#Audio_Object_Types
};


class AudioConfig {
public:
	AudioConfig() = default;
	explicit AudioConfig(const Atom& stsd);

	bool is_ok = false;

	uint8_t object_type_id_;
	uint8_t stream_type_;
	uint buffer_size_;
	uint max_bitrate_;
	uint avg_bitrate_;

	uint8_t object_type_;
	uint8_t frequency_index_;
	uint8_t channel_config_;
	bool frame_length_flag;  // false = 1024; true = 960.

private:
	bool decode(const uchar* start);
};


// vim:set ts=4 sw=4 sts=4 noet:
#endif  // AUDIO_CONFIG_H_
