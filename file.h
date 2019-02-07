/*
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

														*/

#ifndef FILE_H
#define FILE_H

extern "C" {
#include <stdint.h>
#include <stdlib.h>
}
#include <stdio.h>
#include <vector>
#include <string>

#include "common.h"

class FileRead {
public:
	FileRead();
	~FileRead();
	bool open(std::string filename);
	bool create(std::string filename);

	void seek(off64_t p);
	off64_t pos();
	bool atEnd();
	off64_t length() { return size_; }

	size_t readBuffer(uchar* target, size_t size, size_t n);

	uint readInt();
	int64_t readInt64();
	void readChar(char *dest, size_t n);
	std::vector<uchar> read(size_t n);

	const uchar* getPtr(int size_requested);
	const uchar* getPtr2(int size_requested);  // changes state (buf_off_)
	ssize_t buf_size_ = 15*(1<<20); // 15 MB

protected:
	size_t fillBuffer(off64_t location);
	uchar* buffer_;
	off64_t size_;
	FILE *file_;
	off64_t buf_begin_;
	off64_t buf_off_;
};

class FileWrite {
public:
	FileWrite();
	~FileWrite();
	bool create(std::string filename);

	off64_t pos();

	int writeInt(int n);
	int writeInt64(int64_t n);
	int writeChar(const uchar *source, size_t n);
	int writeChar(const char *source, size_t n);
	int write(std::vector<uchar> &v);

protected:
	FILE *file_;
};

#endif // FILE_H
