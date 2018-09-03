//==================================================================//
/*                                                                  *
	Untrunc - file.cpp

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
 *                                                                  */
//==================================================================//

#include "file.h"

#include <cassert>
#include <cstring>
#include <algorithm>
#include <limits>


using std::min;
using std::string;
using std::vector;


// Seek from end-of-file when seeking to a negative offset.
//#define FILE_SEEK_FROM_END  1


// Code requires 8-bit bytes.
static_assert(sizeof(uint8_t) == sizeof(char) && sizeof(uint8_t) == 1 &&
			  sizeof(uint32_t) == 4 &&
			  std::numeric_limits<unsigned char>::digits == 8,
			  "Unsupported machine byte size");


// Redirect C files.
FileRedirect::FileRedirect(std::FILE*& file, std::FILE* to_file)
	: file_ref(file), file_value(file) {
	file = to_file;
	if (file_ref) std::fflush(file_ref);
}

FileRedirect::~FileRedirect() {
	if (file_ref) std::fflush(file_ref);
	file_ref = file_value;
}


// RAII wrapper for FILE, to make sure it gets closed if an error occurs.
void File::FileCloser::operator()(std::FILE *fp) const {
	fclose(fp);
}


// Reading from a file.
FileRead::FileRead(const string& filename) {
	if (!filename.empty() && !open(filename))
		throw string("Could not open file: ") + filename;
}

bool FileRead::open(const string& filename, bool buffered) {
	logg(VV, ((buffered) ? "open buffered file: ": "open unbuffered file: "),
		 filename, ".\n");
	close();

	if (filename.empty()) return false;
	file_.reset(fopen(filename.c_str(), "rb"));
	if (!file_) {
		int rc = errno;
		logg(E, "failed to open: ", filename, " (errno=", rc, ").\n");
		return false;
	}
	name_ = filename;

	fseeko(file_.get(), 0L, SEEK_END);
	off_t sz = ftello(file_.get());
	fseeko(file_.get(), 0L, SEEK_SET);
	if (sz < 0) return false;
	file_size_ = sz;

	if (buffered) {
		static_assert(kBufSize < std::numeric_limits<off_t>::max());
		buffer_.resize(kBufSize);
		if (fillBuffer(-1) == 0 && file_size_ > 0) return false;
	}
	return true;
}

void FileRead::close() {
	file_.reset();
	file_size_      = -1;  // Size of the file.
	buf_begin_pos_  = -1;  // File position of beginning of the buffer.
	buf_size_       =  0;  // Size of buffered data.
	buf_index_      =  0;  // Index into the buffer.
	buffer_.clear();
	name_.clear();
}


void FileRead::seek(off_t pos) {
	if (!file_) return;
#ifdef FILE_SEEK_FROM_END
	logg(VV, ((pos < 0) ? "seek file from end: " : "seek file to position: "),
		 pos, ".\n");

	// Convert to absolute file position.
	if (pos < 0) {
		if (file_size_ < 0 || (pos += file_size_) < 0)
			pos = 0;
	}
#else
	logg(VV, "seek file to position: ", pos, ".\n");

	if (pos < 0) {
		logg(E, "failed to seek to position: ", pos, ".\n");
		return;
	}
#endif

	if (pos >= buf_begin_pos_ && pos < buf_end_pos()) {
		buf_index_ = static_cast<size_t>(pos - buf_begin_pos_);
	} else {
		if (fillBuffer(pos) == 0 && buffer_.empty()) {
			if (fseeko(file_.get(), pos, SEEK_SET) < 0) {
				int rc = errno;
				logg(E, "failed to seek to position: ", pos, " (errno=", rc, ").\n");
			}
		}
	}
}

void FileRead::rewind() {
	if (!file_) {
		close();          // Just in case.
		return;
	}
	logg(VV, "rewind file.\n");

	buf_begin_pos_ = -1;
	buf_size_      =  0;
	buf_index_     =  0;
	clearerr(file_.get());
	if (fillBuffer(0) == 0) {
		clearerr(file_.get());
		if (fseeko(file_.get(), 0L, SEEK_SET) < 0) {
			int rc = errno;
			logg(E, "failed to seek to position: 0 (errno=", rc, ").\n");
		}
	}
	clearerr(file_.get());
	fflush(file_.get());  // Sync std::FILE buffer to file position.
	clearerr(file_.get());
}

off_t FileRead::pos() {
	if (!file_) return -1;

	off_t pos = (!buffer_.empty()) ? buf_current_pos() : off_t(-1);
	return (pos >= 0) ? pos : ftello(file_.get());
}


bool FileRead::atEnd() {
	off_t pos = this->pos();
	return (pos < 0 || pos >= file_size_);
}


// Fill the buffer with file data starting at file position 'pos'.
// Return: 0 on failure and size of buffered data on success (which could be 0).
//
// Show buffer changes in fillBuffer().
//#define FILE_FILLBUFFER_PRINT 1
size_t FileRead::fillBuffer(off_t pos) {
	if (!file_ || buffer_.empty()) return 0;
	logg(VV, "fill the file buffer from position: ", pos, ".\n");
#ifdef FILE_FILLBUFFER_PRINT
	hexDump(&buffer_[buf_index_], min(buf_size_ - buf_index_, size_t(32)),
			"FileRead: Fill Buffer Before:", buf_index_);
#endif

	size_t avail = 0;
	if (pos < 0) {
		pos = ftello(file_.get());
		if (pos < 0) {
			int rc = errno;
			logg(E, "failed to tell the position (errno=", rc, ").\n");
			return 0;
		}
	} else if (buf_begin_pos_ < 0 ||
			   pos <= buf_begin_pos_ || pos > buf_end_pos()) {
		// Re-read buffer data if pos == buf_begin_pos_.
		if (fseeko(file_.get(), pos, SEEK_SET) < 0) {
			int rc = errno;
			logg(E, "failed to seek to position: ", pos,
				 " (errno=", rc, ").\n");
			return 0;
		}
	} else {
		off_t index = pos - buf_begin_pos_;  // 0 < index <= buf_size_
		avail = buf_size_ - index;
		if (avail > 0)
			memmove(buffer_.data(), &buffer_[index], avail);
	}
	buf_begin_pos_ = pos;
	buf_size_      = avail;
	buf_index_     = 0;
	size_t fill_size = buffer_.size() - avail;
	size_t len = fread(&buffer_[avail], sizeof(uchar), fill_size, file_.get());
	buf_size_ += len;
	if (len != fill_size) {
		if (ferror(file_.get()))
			logg(E, "failed to read (", len, " < ", fill_size, ").\n");
		if (buf_size_ < buffer_.size())
			memset(&buffer_[buf_size_], 0, buffer_.size() - buf_size_);
	}
#ifdef FILE_FILLBUFFER_PRINT
	hexDump(&buffer_[buf_index_], min(buf_size_ - buf_index_, size_t(32)),
			"FileRead: Fill Buffer After:", buf_index_);
#endif
	return buf_size_;
}

size_t FileRead::readBuffer(uchar* dest, size_t size, size_t n) {
	assert((dest || n == 0) && size > 0 &&
		   (std::numeric_limits<size_t>::max() / size) >= n);
	logg(VV, "read buffer: ", size * n, " at position: ", buf_current_pos(),
		 " at index: ", buf_index_, '\n');

	size_t total = size * n;
	if (total == 0) return 0;
	size_t avail = buf_size_ - buf_index_;
	if (total <= avail) {
		memcpy(dest, &buffer_[buf_index_], total);
		buf_index_ += total;
		return n;
	}
	if (avail > 0) {
		memcpy(dest, &buffer_[buf_index_], avail);
		buf_index_  = buf_size_;
		dest       += avail;
		total      -= avail;
	}
	size_t nread = avail;
	if (total < buffer_.size()) {
		avail = fillBuffer(buf_end_pos());
		size_t len = min(avail, total);
		memcpy(dest, buffer_.data(), len);
		buf_index_ += len;
		nread      += len;
	} else if (file_) {
		if (!buffer_.empty()) logg(VV, "reading data unbuffered.\n");
		size_t len = fread(dest, sizeof(uchar), total, file_.get());
		nread += len;
		if (len != total) {
			if (ferror(file_.get()))
				logg(E, "failed to read (", len, " < ", total, ").\n");
		}
		fillBuffer(-1);
	}
	return nread / size;
}

const uchar* FileRead::getPtr(size_t size) {
	logg(VV, "get read pointer: ", size,
		 " at position: ", buf_current_pos(), '\n');
	if (!buffer_.empty()) {
		size_t avail = buf_size_ - buf_index_;
		if (size >  avail)
			avail = fillBuffer(buf_current_pos());
		if (size <= avail)
			return &buffer_[buf_index_];
	}
	logg(E, "failed to get read pointer: ", size,
		 " at position: ", buf_current_pos(), ".\n");
	throw string("Could not get read pointer");
	return nullptr;
}

const uchar* FileRead::getPtr(off_t pos, size_t size) {
	logg(VV, "get read pointer: ", size, " from position: ", pos, '\n');
	if (pos < 0)
		pos = buf_current_pos();
	if (pos >= 0 && !buffer_.empty()) {
		if (pos >= buf_begin_pos_ && pos < buf_end_pos()) {
			buf_index_ = static_cast<size_t>(pos - buf_begin_pos_);
			size_t avail = buf_size_ - buf_index_;
			if (size <= avail)
				return &buffer_[buf_index_];
		}
		size_t avail = fillBuffer(pos);
		if (size <= avail)
			return &buffer_[buf_index_];
	}
	logg(E, "failed to get read pointer: ", size,
		 " from position: ", pos, ".\n");
	throw string("Could not get read pointer");
	return nullptr;
}


uint8_t FileRead::readUint8() {
	uint8_t data = 0;
	size_t len = readBuffer(&data, sizeof(data), 1);
	if (len != 1) {
		logg(E, "failed to read 8-bit value at position: ", buf_current_pos(),
			 ".\n");
		throw string("Could not read 8-bit value");
	}
	return data;
}

uint16_t FileRead::readUint16() {
	uint8_t data[sizeof(uint16_t)] = {};
	size_t len = readBuffer(data, sizeof(data), 1);
	if (len != 1) {
		logg(E, "failed to read 16-bit value at position: ", buf_current_pos(),
			 ".\n");
		throw string("Could not read 16-bit value");
	}
	return readBE<uint16_t>(data);
}

uint32_t FileRead::readUint24() {
	uint8_t data[sizeof(uint8_t) * 3] = {};
	size_t len = readBuffer(data, sizeof(data), 1);
	if (len != 1) {
		logg(E, "failed to read 24-bit value at position: ", buf_current_pos(),
			 ".\n");
		throw string("Could not read 24-bit value");
	}
	return readBE<uint32_t,sizeof(data)>(data);
}

uint32_t FileRead::readUint32() {
	uint8_t data[sizeof(uint32_t)] = {};
	size_t len = readBuffer(data, sizeof(data), 1);
	if (len != 1) {
		logg(E, "failed to read 32-bit value at position: ", buf_current_pos(),
			 ".\n");
		throw string("Could not read 32-bit value");
	}
	return readBE<uint32_t>(data);
}

uint64_t FileRead::readUint64() {
	uint8_t data[sizeof(uint64_t)] = {};
	size_t len = readBuffer(data, sizeof(data), 1);
	if (len != 1) {
		logg(E, "failed to read 64-bit value at position: ", buf_current_pos(),
			 ".\n");
		throw string("Could not read 64-bit value");
	}
	return readBE<uint64_t>(data);
}

void FileRead::readChar(char* dest, size_t n) {
	assert(dest || n == 0);
	if (n > 0) {
		uchar* udest = reinterpret_cast<uchar*>(dest);
		size_t len = readBuffer(udest, sizeof(uchar), n);
		if (len != n) {
			logg(E, "failed to read chars at position: ", buf_current_pos(),
				 " (", len, " < ", n, ").\n");
			throw string("Could not read chars");
		}
	}
}

vector<uchar> FileRead::read(size_t n) {
	vector<uchar> dest(n);
	if (n > 0) {
		size_t len = readBuffer(dest.data(), sizeof(uchar), n);
		if (len != n) {
			logg(E, "failed to read at position: ", buf_current_pos(),
				 " (", len, " < ", n, ").\n");
			throw string("Could not read at position");
		}
	}
	return dest;
}


// Writing to a file.
FileWrite::FileWrite(const string& filename) {
	if (!filename.empty() && !create(filename))
		throw string("Could not create file: ") + filename;
}

bool FileWrite::create(const string& filename) {
	file_.reset();
	name_.clear();

	if (filename.empty()) return false;
	file_.reset(fopen(filename.c_str(), "wb"));
	if (!file_) {
		int rc = errno;
		logg(E, "failed to create: ", filename, " (errno=", rc, ").\n");
		return false;
	}
	name_ = filename;
	return true;
}


off_t FileWrite::pos() {
	return (file_) ? ftello(file_.get()) : off_t(-1);
}

off_t FileWrite::size() {
	if (!file_)  return -1;
	off_t pos = ftello(file_.get());
	if (pos < 0) return -1;
	fseeko(file_.get(), 0L,  SEEK_END);
	off_t sz  = ftello(file_.get());
	fseeko(file_.get(), pos, SEEK_SET);
	if (sz  < 0) return -1;
	return sz;
}


ssize_t FileWrite::writeUint8(uint8_t value) {
	if (!file_) return -1;

	size_t len = fwrite(&value, sizeof(value), 1, file_.get());
	if (len != 1) {
		logg(E, "failed to write 8-bit value.\n");
		if (ferror(file_.get())) return -1;
	}
	return len;
}

ssize_t FileWrite::writeUint16(uint16_t value) {
	if (!file_) return -1;

	uint8_t data[sizeof(value)] = {};
	writeBE(data, value);

	size_t len = fwrite(data, sizeof(data), 1, file_.get());
	if (len != 1) {
		logg(E, "failed to write 16-bit value.\n");
		if (ferror(file_.get())) return -1;
	}
	return len;
}

ssize_t FileWrite::writeUint24(uint32_t value) {
	if (!file_) return -1;

	uint8_t data[sizeof(uint8_t) * 3] = {};
	writeBE<uint32_t,sizeof(data)>(data, value);

	size_t len = fwrite(data, sizeof(data), 1, file_.get());
	if (len != 1) {
		logg(E, "failed to write 24-bit value.\n");
		if (ferror(file_.get())) return -1;
	}
	return len;
}

ssize_t FileWrite::writeUint32(uint32_t value) {
	if (!file_) return -1;

	uint8_t data[sizeof(value)] = {};
	writeBE(data, value);

	size_t len = fwrite(data, sizeof(data), 1, file_.get());
	if (len != 1) {
		logg(E, "failed to write 32-bit value.\n");
		if (ferror(file_.get())) return -1;
	}
	return len;
}

ssize_t FileWrite::writeUint64(uint64_t value) {
	if (!file_) return -1;

	uint8_t data[sizeof(value)] = {};
	writeBE(data, value);

	size_t len = fwrite(data, sizeof(data), 1, file_.get());
	if (len != 1) {
		logg(E, "failed to write 64-bit value.\n");
		if (ferror(file_.get())) return -1;
	}
	return len;
}

ssize_t FileWrite::writeChar(const char* source, size_t n) {
	assert(source || n == 0);
	assert(n <= std::numeric_limits<ssize_t>::max() / sizeof(char));
	if (n == 0) return  0;
	if (!file_) return -1;

	size_t len = fwrite(source, sizeof(char), n, file_.get());
	if (len != n) {
		logg(E, "failed to write chars (", len, " < ", n, ").\n");
		if (ferror(file_.get())) return -1;
	}
	return len;
}

ssize_t FileWrite::write(const vector<uchar>& v) {
	assert(v.size() <= std::numeric_limits<ssize_t>::max() / sizeof(uchar));
	if (v.empty()) return  0;
	if (!file_)    return -1;

	size_t len = fwrite(v.data(), sizeof(uchar), v.size(), file_.get());
	if (len != v.size()) {
		logg(E, "failed to write at position (", len, " < ", v.size(), ").\n");
		if (ferror(file_.get())) return -1;
	}
	return len;
}


// vim:set ts=4 sw=4 sts=4 noet:
