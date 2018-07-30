#ifndef AUDIOCONFIG_H
#define AUDIOCONFIG_H

#include "atom.h"

/* Audio Object types */
enum {
	AAC_MAIN             = 1,
	AAC_LC             = 2, // Low Complexity
	AAC_SSR             = 3, // Scaleable Sampe Rate
	AAC_LTP             = 4, // Long Term Prediction
	// ...
	// see: https://wiki.multimedia.cx/index.php/MPEG-4_Audio#Audio_Object_Types
};

class AudioConfig
{
public:
	AudioConfig() = default;
	AudioConfig(const Atom& stsd);

	bool is_ok = false;

	uint8_t object_type_id_;
	uint8_t stream_type_;
	uint buffer_size_;
	uint max_bitrate_;
	uint avg_bitrate_;

	uint8_t object_type_;
	uint8_t frequency_index_;
	uint8_t channel_config_;
	bool frame_length_flag; // false = 1024; true = 960


private:
	bool decode(const uchar* start);
};

#endif // AUDIOCONFIG_H
