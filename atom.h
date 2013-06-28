#ifndef ATOM_H
#define ATOM_H

#include <vector>
#include <string>

class File;

class Atom {
public:
    int start;       //including 8 header bytes
    int length;      //including 8 header bytes
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
    ~Atom();

    void parseHeader(File &file); //read just name and length
    void parse(File &file);
    void write(File &file);
    void print(int offset);

    std::vector<Atom *> atomsByName(std::string name);
    Atom *atomByName(std::string name);

    void prune(std::string name);
    void updateLength();

    static bool isParent(char *id);
    static bool isDual(char *id);
    static bool isVersioned(char *id);

    int readInt(int offset);
    void writeInt(int value, int offset);
    void readChar(char *str, int offset, int length);

};

#endif // ATOM_H
