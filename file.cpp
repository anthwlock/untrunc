#include "file.h"
#include <string>

using namespace std;

static void reverse(int &input) {
    int output;
    char *a = ( char* )&input;
    char *b = ( char* )&output;

    b[0] = a[3];
    b[1] = a[2];
    b[2] = a[1];
    b[3] = a[0];
    input = output;
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

void File::seek(int p) {
    fseek(file, p, SEEK_SET);
}

int File::pos() {
    return ftell(file);
}

bool File::atEnd() {
    int pos = ftell(file);
    return pos == size;
}

int File::readInt() {
    int value;
    int n = fread(&value, sizeof(int), 1, file);
    if(n != 1)
        throw string("Could not read atom length");
    reverse(value);
    return value;
}

int File::readInt64() {
    int hi, low;
    int n = fread(&hi, sizeof(int), 1, file);
    if(n != 1)
        throw string("Could not read atom length");
    n = fread(&low, sizeof(int), 1, file);
    if(n != 1)
        throw string("Could not read atom length");

    reverse(low);
    return low;
}

void File::readChar(char *dest, int n) {
    int len = fread(dest, sizeof(char), n, file);
    if(len != n)
        throw string("Could not read chars");
}

vector<unsigned char> File::read(int n) {
    vector<unsigned char> dest(n);
    int len = fread(&*dest.begin(), sizeof(unsigned char), n, file);
    if(len != n)
        throw string("Could not read chars");
    return dest;
}

int File::writeInt(int n) {
    reverse(n);
    fwrite(&n, sizeof(int), 1, file);
    return 4;
}

int File::writeInt64(int n) {
    int hi = 0;
    reverse(n);
    fwrite(&hi, sizeof(int), 1, file);
    fwrite(&n, sizeof(int), 1, file);
    return 8;
}

int File::writeChar(char *source, int n) {
    fwrite(source, 1, n, file);
    return n;
}

int File::write(vector<unsigned char> &v) {
    fwrite(&*v.begin(), 1, v.size(), file);
    return v.size();
}
