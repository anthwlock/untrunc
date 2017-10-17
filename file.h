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
}
#include <stdio.h>
#include <vector>
#include <string>

uint16_t swap16(uint16_t us);
uint32_t swap32(uint32_t ui);
uint64_t swap64(uint64_t ull);

class File {
public:
	File();
	~File();
	bool open(std::string filename);
	bool create(std::string filename);

	void seek(off_t p);
	off_t pos();
	bool atEnd();
	off_t length() { return size; }

	int readInt();
	int64_t readInt64();
	void readChar(char *dest, size_t n);
	std::vector<unsigned char> read(size_t n);

	int writeInt(int n);
	int writeInt64(int64_t n);
	int writeChar(char *source, size_t n);
	int write(std::vector<unsigned char> &v);

protected:
	off_t size;
	FILE *file;
};

#endif // FILE_H
