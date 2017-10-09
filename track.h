/*
    Untrunc - track.h

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
	int getLength(unsigned char *start, int maxlength, int &duration);
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
