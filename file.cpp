/*
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
                                                                                
                                                        */

#include "file.h"
#include <string>
#include <endian.h>

using namespace std;


uint16_t swap16(uint16_t us) {
	return (us >> 8) | (us << 8);
}

uint32_t swap32(uint32_t ui) {
	return (ui >> 24) | ((ui<<8) & 0x00FF0000) | ((ui>>8) & 0x0000FF00) | (ui << 24);
}

uint64_t swap64(uint64_t ull) {
	return (ull >> 56) |
			((ull<<40) & 0x00FF000000000000) |
			((ull<<24) & 0x0000FF0000000000) |
			((ull<<8)  & 0x000000FF00000000) |
			((ull>>8)  & 0x00000000FF000000) |
			((ull>>24) & 0x0000000000FF0000) |
			((ull>>40) & 0x000000000000FF00) |
			(ull << 56);
}



File::File(): file(NULL) {
}

File::~File() {
    if(file)
        fclose(file);
}


bool File::open(string filename) {
    file = fopen(filename.c_str(), "r");
    if(file == NULL) return false;

    fseeko(file, 0L, SEEK_END);
    size = ftello(file);
    fseeko(file, 0L, SEEK_SET);

    return true;
}

bool File::create(string filename) {
    file = fopen(filename.c_str(), "wb");
    if(file == NULL) return false;
    return true;
}

void File::seek(off_t p) {
    fseeko(file, p, SEEK_SET);
}

off_t File::pos() {
    return ftello(file);
}

bool File::atEnd() {
    return ftello(file) == size;
}

int File::readInt() {
    int value;
    int n = fread(&value, sizeof(int), 1, file);
    if(n != 1)
        throw string("Could not read atom length");
	return swap32(value);
}

int64_t File::readInt64() {
    int64_t value;
    int n = fread(&value, sizeof(value), 1, file);
    if(n != 1)
        throw string("Could not read atom length");

	return swap64(value);
}

void File::readChar(char *dest, size_t n) {
    size_t len = fread(dest, sizeof(char), n, file);
    if(len != n)
        throw string("Could not read chars");
}

vector<unsigned char> File::read(size_t n) {
    vector<unsigned char> dest(n);
    size_t len = fread(&*dest.begin(), sizeof(unsigned char), n, file);
    if(len != n)
        throw string("Could not read at position");
    return dest;
}

int File::writeInt(int n) {
	n = swap32(n);
    fwrite(&n, sizeof(int), 1, file);
    return 4;
}

int File::writeInt64(int64_t n) {
	n = swap64(n);
    fwrite(&n, sizeof(n), 1, file);
    return 8;
}

int File::writeChar(char *source, size_t n) {
    fwrite(source, 1, n, file);
    return n;
}

int File::write(vector<unsigned char> &v) {
    fwrite(&*v.begin(), 1, v.size(), file);
    return v.size();
}
