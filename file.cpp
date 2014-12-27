#include "file.h"
#include <string>
#include <endian.h>

using namespace std;

File::File(): file(NULL) {
}

File::~File() {
    if(file)
        fclose(file);
}


bool File::open(string filename) {
    file = fopen(filename.c_str(), "r");
    if(file == NULL) return false;

    fseek(file, 0L, SEEK_END);
    size = ftell(file);
    fseek(file, 0L, SEEK_SET);

    return true;
}

bool File::create(string filename) {
    file = fopen(filename.c_str(), "wb");
    if(file == NULL) return false;
    return true;
}

void File::seek(int64_t p) {
    fseek(file, p, SEEK_SET);    
}

int64_t File::pos() {
    return ftell(file);
}

bool File::atEnd() {
    long pos = ftell(file);
    return pos == size;
}

int File::readInt() {
    int value;
    int n = fread(&value, sizeof(int), 1, file);
    if(n != 1)
        throw string("Could not read atom length");
    return be32toh(value);
}

int64_t File::readInt64() {
    int64_t value;
    int n = fread(&value, sizeof(value), 1, file);
    if(n != 1)
        throw string("Could not read atom length");

    return be64toh(value);
}

void File::readChar(char *dest, int64_t n) {
    int len = fread(dest, sizeof(char), n, file);
    if(len != n)
        throw string("Could not read chars");
}

vector<unsigned char> File::read(int64_t n) {
    vector<unsigned char> dest(n);
    long len = fread(&*dest.begin(), sizeof(unsigned char), n, file);
    if(len != n)
        throw string("Could not read at position");
    return dest;
}

int File::writeInt(int n) {
    n = htobe32(n);
    fwrite(&n, sizeof(int), 1, file);
    return 4;
}

int File::writeInt64(int64_t n) {
    n = htobe64(n);
    fwrite(&n, sizeof(n), 1, file);
    return 8;
}

int File::writeChar(char *source, int64_t n) {
    fwrite(source, 1, n, file);
    return n;
}

int File::write(vector<unsigned char> &v) {
    fwrite(&*v.begin(), 1, v.size(), file);
    return v.size();
}
