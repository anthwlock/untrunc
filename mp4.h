#ifndef MP4_H
#define MP4_H

#include <vector>
#include <string>
#include <stdio.h>

#include "track.h"
class Atom;
class File;
class AVFormatContext;

class Mp4 {
public:
    int timescale;
    int duration;

    Mp4();
    ~Mp4();

    void open(std::string filename);

    void printMediaInfo();
    void printAtoms();
    void saveVideo(std::string filename);

    void analyze();
    void writeTracksToAtoms();
    void repair(std::string filename);

protected:
    Atom *root;
    std::vector<Track> tracks;
    AVFormatContext *context;

    void parseTracks();
};


#endif // MP4_H
