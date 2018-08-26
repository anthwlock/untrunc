//==================================================================//
/*                                                                  *
	Untrunc - file.h

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

#ifndef FILE_H_
#define FILE_H_

#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>
#include <memory>

#include "common.h"


// RAII wrapper for FILE, to make sure it gets closed.
class File {
protected:
	struct FileCloser {
		constexpr FileCloser()                  noexcept = default;
		constexpr FileCloser(const FileCloser&) noexcept = default;

		void operator()(std::FILE *fp) const;
	};

	std::unique_ptr<std::FILE, File::FileCloser> file_;
};


// Reading from a file.
class FileRead : protected File {
public:
	static const size_t kBufSize = 15UL*1024*1024;  // 15 MiB.

	FileRead() = default;
	explicit FileRead(const std::string& filename);

	bool open(const std::string& filename, bool buffered = true);

	explicit operator bool() const { return static_cast<bool>(file_); }

	off_t pos();
	void  seek(off_t pos);
	void  rewind();
	bool  atEnd();
	off_t size()   const { return file_size_; }
	off_t length() const { return size(); }

	uint8_t  readUint8();
	uint16_t readUint16();
	uint32_t readUint24();
	uint32_t readUint32();
	uint64_t readUint64();
	int32_t  readInt32() { return readUint32(); }
	int64_t  readInt64() { return readUint64(); }
	void     readChar(char* dest, size_t n);
	std::vector<uchar> read(size_t n);

	const uchar* getPtr(size_t size);
	const uchar* getPtr(off_t pos, size_t size);

private:
	off_t  file_size_     = -1;
	std::vector<uchar> buffer_;
	off_t  buf_begin_pos_ = -1;
	size_t buf_size_      =  0;
	size_t buf_index_     =  0;

	off_t  buf_end_pos()     const { return buf_begin_pos_ + buf_size_;  }
	off_t  buf_current_pos() const { return buf_begin_pos_ + buf_index_; }

	void   close();
	size_t fillBuffer(off_t pos);
	size_t readBuffer(uchar* target, size_t size, size_t n);
};


// Writing to a file.
class FileWrite : protected File {
public:
	FileWrite() = default;
	explicit FileWrite(const std::string& filename);

	bool create(const std::string& filename);

	explicit operator bool() const { return static_cast<bool>(file_); }

	off_t pos();
	off_t size();
	off_t length() { return size(); }

	ssize_t writeUint8(uint8_t value);
	ssize_t writeUint16(uint16_t value);
	ssize_t writeUint24(uint32_t value);
	ssize_t writeUint32(uint32_t value);
	ssize_t writeUint64(uint64_t value);
	ssize_t writeInt32(int32_t value) { return writeUint32(value); }
	ssize_t writeInt64(int64_t value) { return writeUint64(value); }
	ssize_t writeChar(const char* source, size_t n);
	ssize_t write(const std::vector<uchar>& v);
};


// vim:set ts=4 sw=4 sts=4 noet:
#endif  // FILE_H_
