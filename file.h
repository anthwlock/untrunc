#ifndef FILE_H
#define FILE_H

extern "C" {
#include <stdint.h>
}
#include <stdio.h>
#include <vector>
#include <string>

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
