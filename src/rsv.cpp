#include "rsv.h"


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