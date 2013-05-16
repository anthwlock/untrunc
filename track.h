#ifndef TRACK_H
#define TRACK_H

#include <vector>
#include <string>

class Atom;
class AVCodecContext;
class AVCodec;

class Codec {
public:
    std::string name;
    void parse(Atom *trak, std::vector<int> &offsets, Atom *mdat);
    bool matchSample(unsigned char *start, int maxlength);
    int getLength(unsigned char *start, int maxlength);
    bool isKeyframe(unsigned char *start, int maxlength);
    //used by: mp4a
    int mask1;
    int mask0;
    AVCodecContext *context;
    AVCodec *codec;
};

class Track {
public:
    Atom *trak;
    int timescale;
    int duration;
    Codec codec;

    std::vector<int> times;
    std::vector<int> offsets;
    std::vector<int> sizes;
    std::vector<int> keyframes; //0 based!

    Track(): trak(0) {}
    void parse(Atom *trak, Atom *mdat);
    void writeToAtoms();
    void clear();
    void fixTimes();

    std::vector<int> getSampleTimes(Atom *t);
    std::vector<int> getKeyframes(Atom *t);
    std::vector<int> getSampleSizes(Atom *t);
    std::vector<int> getChunkOffsets(Atom *t);
    std::vector<int> getSampleToChunk(Atom *t, int nchunks);

    void saveSampleTimes();
    void saveKeyframes();
    void saveSampleToChunk();
    void saveSampleSizes();
    void saveChunkOffsets();

};

#endif // TRACK_H
