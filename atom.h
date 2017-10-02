#ifndef ATOM_H
#define ATOM_H
extern "C" {
    #include <stdint.h>
}
#include <vector>
#include <string>

#include "file.h"
class Atom {
public:
    int64_t start;       //including 8 header bytes
    int64_t length;      //including 8 header bytes
    char name[5];
    char head[4];
    char version[4];
    std::vector<unsigned char> content;
    std::vector<Atom *> children;

    Atom(): start(0), length(-1) {
        name[0] = name[1] = name[2] = name[3] = name[4] = 0;
        length = 0;
        start = 0;
    }
	virtual ~Atom();

    void parseHeader(File &file); //read just name and length
    void parse(File &file);
    virtual void write(File &file);
    void print(int offset);

    std::vector<Atom *> atomsByName(std::string name);
    Atom *atomByName(std::string name);
    void replace(Atom *original, Atom *replacement);

    void prune(std::string name);
    virtual void updateLength();

    virtual int64_t contentSize() { return content.size(); }

    static bool isParent(char *id);
    static bool isDual(char *id);
    static bool isVersioned(char *id);

    virtual int readInt(int64_t offset);
    void writeInt(int value, int64_t offset);
    void readChar(char *str, int64_t offset, int64_t length);

};

class BufferedAtom: public Atom {
public:
    File file;
    int64_t file_begin;
    int64_t file_end;
    unsigned char *buffer;
    int64_t buffer_begin;
    int64_t buffer_end;

    BufferedAtom(std::string filename);
    ~BufferedAtom();
    unsigned char *getFragment(int64_t offset, int64_t size);
    int64_t contentSize() { return file_end - file_begin; }
    void updateLength();

    int readInt(int64_t offset);
    void write(File &file);

};

#endif // ATOM_H
