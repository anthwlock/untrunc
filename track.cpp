#include "track.h"
#include "atom.h"

#include <iostream>
#include <vector>
#include <string.h>
#include <assert.h>

#define __STDC_LIMIT_MACROS 1
#define __STDC_CONSTANT_MACROS 1

extern "C" {
#ifndef INT64_C
#define INT64_C(c) (c ## LL)
#define UINT64_C(c) (c ## ULL)
#endif

#include <stdint.h>
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
}

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

using namespace std;


void Codec::parse(Atom *trak, vector<int> &offsets, Atom *mdat) {
    Atom *stsd = trak->atomByName("stsd");
    int entries = stsd->readInt(4);
    if(entries != 1)
        throw string("Multiplexed stream! Not supported");

    char _codec[5];
    stsd->readChar(_codec, 12, 4);
    name = _codec;


    //this was a stupid attempt at trying to detect packet type based on bitmasks
    mask1 = 0xffffffff;
    mask0 = 0xffffffff;
    //build the mask:
    for(int i = 0; i < offsets.size(); i++) {
        int offset = offsets[i];
        if(offset < mdat->start || offset - mdat->start > mdat->length) {
            cout << "Invalid offset in track!\n";
            exit(0);
        }

        int s = mdat->readInt(offset - mdat->start - 8);
        reverse(s);
        mask1 &= s;
        mask0 &= ~s;

        assert((s & mask1) == mask1);
        assert((~s & mask0) == mask0);
    }
}

#define VERBOSE 1

bool Codec::matchSample(unsigned char *start, int maxlength) {
    int s = *(int *)start;

    if(name == "avc1") {

//this works only for a very specific kind of video
//#define SPECIAL_VIDEO
#ifdef SPECIAL_VIDEO
        int s2 = ((int *)start)[1];
        if(s !=  0x02000000  || (s2 != 0x00003009 && s2 != 0x00001009)) return false;
        return true;
#endif


        //TODO use the first byte of the nal: forbidden bit and type!
        int nal_type = (start[4] & 0x1f);
        //the other values are really uncommon on cameras...
        if(nal_type != 1 && nal_type != 5 && nal_type != 6 && nal_type != 7 && nal_type != 8 && nal_type != 9) {
#ifdef VERBOSE
            cout << "avc1: no match beacuse of nal type: " << nal_type << endl;
#endif
            return false;
        }
        //if nal is equal 7, the other fragments (starting with nal type 7)
        //should be part of the same packet
        //(we cannot recover time information, remember)
        if(start[0] == 0) {
#ifdef VERBOSE
            cout << "avc1: Match with 0 header\n";
#endif
            return true;
        }
#ifdef VERBOSE
        cout << "avc1: failed for not particular reason\n";
#endif
        return false;

    } else if(name == "mp4a") {
        reverse(s);
        if(s > 1000000) {
#ifdef VERBOSE
            cout << "mp4a: Success because of large s value\n";
#endif
            return true;
        }
        //horrible hack... these values might need to be changed depending on the file
        if((start[4] == 0xee && start[5] == 0x1b) ||
                (start[4] == 0x3e && start[5] == 0x64)) {
            cout << "mp4a: Success because of horrible hack.\n";
            return true;
        }

        if(start[0] == 0) return false;
#ifdef VERBOSE
        cout << "Success for no particular reason....\n";
#endif
        return true;

    } else if(name == "alac") {
        reverse(s);
        int t = *(int *)(start + 4);
        reverse(t);
        t &= 0xffff0000;

        //cout << hex << t << dec << endl;
        if(s == 0 && t == 0x00130000) return true;
        if(s == 0x1000 && t == 0x001a0000) return true;
        return false;

    } else if(name == "samr") {
        return start[0] == 0x3c;
    } else if(name == "twos") {
        //weird audio codec: each packet is 2 signed 16b integers.
        cerr << "This audio codec is EVIL, there is no hope to guess it.\n";
        exit(0);
        return true;
    }
    return false;
}

int Codec::getLength(unsigned char *start, int maxlength) {
    if(name == "mp4a") {
        AVFrame *frame = avcodec_alloc_frame();
        if(!frame)
            throw string("Could not create AVFrame");
        AVPacket avp;
        av_init_packet(&avp);

        int got_frame;
        avp.data=(uint8_t *)(start);
        avp.size = maxlength;
        int consumed = avcodec_decode_audio4(context, frame, &got_frame, &avp);
        av_freep(&frame);
        return consumed;

    } else if(name == "avc1") {

        /* NAL unit types
        enum {
            NAL_SLICE           = 1,
            NAL_DPA             = 2,
            NAL_DPB             = 3,
            NAL_DPC             = 4,
            NAL_IDR_SLICE       = 5,
            NAL_SEI             = 6,
            NAL_SPS             = 7,
            NAL_PPS             = 8,
            NAL_AUD             = 9,
            NAL_END_SEQUENCE    = 10,
            NAL_END_STREAM      = 11,
            NAL_FILLER_DATA     = 12,
            NAL_SPS_EXT         = 13,
            NAL_AUXILIARY_SLICE = 19,
            NAL_FF_IGNORE       = 0xff0f001,
        };
        */
        int first_nal_type = (start[4] & 0x1f);
        cout << "Nal type: " << first_nal_type << endl;
        if(first_nal_type > 8) {
            cout << "Unrecognized nal type: " << first_nal_type << endl;
            return -1;
        }
        int length = *(int *)start;
        reverse(length);

        if(length <= 0) return -1;
        length += 4;

        cout << "Length for first packet = " << length <<  " / " << maxlength << endl;

        if(length > maxlength) return -1;

#define SPLIT_NAL_PACKETS 1
#ifdef SPLIT_NAL_PACKETS
        return length;
#endif

        //consume all nal units where type is != 1, 3, 5
        unsigned char *pos = start;
        bool found = false;
        while(!found) {
            pos = start + length;
            assert(pos - start < maxlength - 4);
            int l = *(int *)pos;
            reverse(l);
            if(l <= 0) break;
            if(pos[0] != 0) break; //not avc1

            int nal_type = (pos[4] & 0x1f);
            cout << "Intermediate nal type: " << nal_type << endl;
            if(nal_type <= 5) found = true;

            //if(nal_type <= 5 || nal_type >= 18) break;//wrong nal or not video
            if(nal_type > 12) break; //unknown nal type
            if(l + length + 8 >= maxlength) break; //out of boundary
            assert(length < maxlength);


            //ok include it
            length += l + 4;
            assert(length + 4 < maxlength);
        }
        return length;
    } else if(name == "samr") { //lenght is multiple of 32, we split packets.
        return 32;
    } else if(name == "twos") { //lenght is multiple of 32, we split packets.
        return 4;
    } else
        return -1;
}

bool Codec::isKeyframe(unsigned char *start, int maxlength) {
    if(name == "avc1") {
        //first byte of the NAL, the last 5 bits determine type (usually 5 for keyframe, 1 for intra frame)
        return (start[4] & 0x1F) == 5;
    } else
        return false;
}




void Track::parse(Atom *t, Atom *mdat) {

    trak = t;
    Atom *mdhd = t->atomByName("mdhd");
    if(!mdhd)
        throw string("No mdhd atom: unknown duration and timescale");

    timescale = mdhd->readInt(12);
    duration = mdhd->readInt(16);

    times = getSampleTimes(t);
    keyframes = getKeyframes(t);
    sizes = getSampleSizes(t);

    vector<int> chunk_offsets = getChunkOffsets(t);
    vector<int> sample_to_chunk = getSampleToChunk(t, chunk_offsets.size());

    if(times.size() != sizes.size()) {
        cout << "Mismatch between time offsets and size offsets: \n";
        cout << "Time offsets: " << times.size() << " Size offsets: " << sizes.size() << endl;
    }
    //assert(times.size() == sizes.size());
    if(times.size() != sample_to_chunk.size()) {
        cout << "Mismatch between time offsets and sample_to_chunk offsets: \n";
        cout << "Time offsets: " << times.size() << " Chunk offsets: " << sample_to_chunk.size() << endl;
    }
    //compute actual offsets
    int old_chunk = -1;
    int offset = -1;
    for(unsigned int i = 0; i < sizes.size(); i++) {
        int chunk = sample_to_chunk[i];
        int size = sizes[i];
        if(chunk != old_chunk) {
            offset = chunk_offsets[chunk];
            old_chunk= chunk;
        }
        offsets.push_back(offset);
        offset += size;
    }

    //move this stuff into track!
    Atom *hdlr = trak->atomByName("hdlr");
    char type[5];
    hdlr->readChar(type, 8, 4);

    bool audio = (type == string("soun"));

    if(type != string("soun") && type != string("vide"))
        return;

    //move this to Codec

    codec.parse(trak, offsets, mdat);
    codec.codec = avcodec_find_decoder(codec.context->codec_id);
    //if audio use next?

    if(!codec.codec) throw string("No codec found!");
    if(avcodec_open2(codec.context, codec.codec, NULL)<0)
        throw string("Could not open codec: ") + codec.context->codec_name;


    /*
    Atom *mdat = root->atomByName("mdat");
    if(!mdat)
        throw string("Missing data atom");

    //print sizes and offsets
        for(int i = 0; i < 10 && i < sizes.size(); i++) {
        cout << "offset: " << offsets[i] << " size: " << sizes[i] << endl;
        cout << mdat->readInt(offsets[i] - (mdat->start + 8)) << endl;
    } */
}

void Track::writeToAtoms() {
    if(!keyframes.size())
        trak->prune("stss");

    saveSampleTimes();
    saveKeyframes();
    saveSampleSizes();
    saveSampleToChunk();
    saveChunkOffsets();

    Atom *mdhd = trak->atomByName("mdhd");
    mdhd->writeInt(duration, 16);

    //Avc1 codec writes something inside stsd.
    //In particular the picture parameter set (PPS) in avcC (after the sequence parameter set)
    //See http://jaadec.sourceforge.net/specs/ISO_14496-15_AVCFF.pdf for the avcC stuff
    //See http://www.iitk.ac.in/mwn/vaibhav/Vaibhav%20Sharma_files/h.264_standard_document.pdf for the PPS
    //I have found out in shane,banana,bruno and nawfel that pic_init_qp_minus26  is different even for consecutive videos of the same camera.
    //the only thing to do then is to test possible values, to do so remove 28 (the nal header) follow the golomb stuff
    //This test could be done automatically when decoding fails..
//#define SHANE 6
#ifdef SHANE
    int pps[15] = {
        0x28ee3880,
        0x28ee1620,
        0x28ee1e20,
        0x28ee0988,
        0x28ee0b88,
        0x28ee0d88,
        0x28ee0f88,
        0x28ee0462,
        0x28ee04e2,
        0x28ee0562,
        0x28ee05e2,
        0x28ee0662,
        0x28ee06e2,
        0x28ee0762,
        0x28ee07e2
    };
    if(codec.name == "avc1") {
        Atom *stsd = trak->atomByName("stsd");
        stsd->writeInt(pps[SHANE], 122); //a bit complicated to find.... find avcC... follow first link.
    }
#endif

}

void Track::clear() {
    //times.clear();
    offsets.clear();
    sizes.clear();
    keyframes.clear();
}

void Track::fixTimes() {
    if(codec.name == "samr") { //just return smallest of packets
        unsigned int min = 0xffffffff;
        for(int i = 0; i < times.size(); i++)
            if(times[i] < min) min = i;
        times.clear();
        times.resize(offsets.size(), 160);
        return;
    }
    while(times.size() < offsets.size())
        times.insert(times.end(), times.begin(), times.end());
    times.resize(offsets.size());

    duration = 0;
    for(unsigned int i = 0; i < times.size(); i++)
        duration += times[i];
}

vector<int> Track::getSampleTimes(Atom *t) {
    vector<int> sample_times;
    //chunk offsets
    Atom *stts = t->atomByName("stts");
    if(!stts)
        throw string("Missing sample to time atom");

    int entries = stts->readInt(4);
    for(int i = 0; i < entries; i++) {
        int nsamples = stts->readInt(8 + 8*i);
        int time = stts->readInt(12 + 8*i);
        for(int i = 0; i < nsamples; i++)
            sample_times.push_back(time);
    }
    return sample_times;
}

vector<int> Track::getKeyframes(Atom *t) {
    vector<int> sample_key;
    //chunk offsets
    Atom *stss = t->atomByName("stss");
    if(!stss)
        return sample_key;

    int entries = stss->readInt(4);
    for(int i = 0; i < entries; i++)
        sample_key.push_back(stss->readInt(8 + 4*i) - 1);
    return sample_key;
}

vector<int> Track::getSampleSizes(Atom *t) {
    vector<int> sample_sizes;
    //chunk offsets
    Atom *stsz = t->atomByName("stsz");
    if(!stsz)
        throw string("Missing sample to sizeatom");

    int entries = stsz->readInt(8);
    int default_size = stsz->readInt(4);
    if(default_size == 0) {
        for(int i = 0; i < entries; i++)
            sample_sizes.push_back(stsz->readInt(12 + 4*i));
    } else {
        sample_sizes.resize(entries, default_size);
    }
    return sample_sizes;
}



vector<int> Track::getChunkOffsets(Atom *t) {
    vector<int> chunk_offsets;
    //chunk offsets
    Atom *stco = t->atomByName("stco");
    if(stco) {
        int nchunks = stco->readInt(4);
        for(int i = 0; i < nchunks; i++)
            chunk_offsets.push_back(stco->readInt(8 + i*4));

    } else {
        Atom *co64 = t->atomByName("co64");
        if(!co64)
            throw string("Missing chunk offset atom");

        int nchunks = co64->readInt(4);
        for(int i = 0; i < nchunks; i++)
            chunk_offsets.push_back(co64->readInt(12 + i*8));
    }
    return chunk_offsets;
}

vector<int> Track::getSampleToChunk(Atom *t, int nchunks){
    vector<int> sample_to_chunk;

    Atom *stsc = t->atomByName("stsc");
    if(!stsc)
        throw string("Missing sample to chunk atom");

    vector<int> first_chunks;
    int entries = stsc->readInt(4);
    for(int i = 0; i < entries; i++)
        first_chunks.push_back(stsc->readInt(8 + 12*i));
    first_chunks.push_back(nchunks+1);

    for(int i = 0; i < entries; i++) {
        int first_chunk = first_chunks[i];
        int last_chunk = first_chunks[i+1];
        int n_samples = stsc->readInt(12 + 12*i);

        for(int k = first_chunk; k < last_chunk; k++) {
            for(int j = 0; j < n_samples; j++)
                sample_to_chunk.push_back(k-1);
        }
    }

    return sample_to_chunk;
}


void Track::saveSampleTimes() {
    Atom *stts = trak->atomByName("stts");
    assert(stts);
    stts->content.resize(4 + //version
                         4 + //entries
                         8*times.size()); //time table
    stts->writeInt(times.size(), 4);
    for(unsigned int i = 0; i < times.size(); i++) {
        stts->writeInt(1, 8 + 8*i);
        stts->writeInt(times[i], 12 + 8*i);
    }
}

void Track::saveKeyframes() {
    Atom *stss = trak->atomByName("stss");
    if(!stss) return;
    assert(keyframes.size());
    if(!keyframes.size()) return;

    stss->content.resize(4 + //version
                         4 + //entries
                         4*keyframes.size()); //time table
    stss->writeInt(keyframes.size(), 4);
    for(unsigned int i = 0; i < keyframes.size(); i++)
        stss->writeInt(keyframes[i] + 1, 8 + 4*i);
}

void Track::saveSampleSizes() {
    Atom *stsz = trak->atomByName("stsz");
    assert(stsz);
    stsz->content.resize(4 + //version
                         4 + //default size
                         4 + //entries
                         4*sizes.size()); //size table
    stsz->writeInt(0, 4);
    stsz->writeInt(sizes.size(), 8);
    for(unsigned int i = 0; i < sizes.size(); i++) {
        stsz->writeInt(sizes[i], 12 + 4*i);
    }
}

void Track::saveSampleToChunk() {
    Atom *stsc = trak->atomByName("stsc");
    assert(stsc);
    stsc->content.resize(4 + // version
                         4 + //number of entries
                         12); //one sample per chunk.
    stsc->writeInt(1, 4);
    stsc->writeInt(1, 8); //first chunk (1 based)
    stsc->writeInt(1, 12); //one sample per chunk
    stsc->writeInt(1, 16); //id 1 (WHAT IS THIS!)
}

void Track::saveChunkOffsets() {
    Atom *co64 = trak->atomByName("co64");
    if(co64) {
        trak->prune("co64");
        Atom *stbl = trak->atomByName("stbl");
        Atom *new_stco = new Atom;
        memcpy(new_stco->name, "stco", 5);
        stbl->children.push_back(new_stco);
    }
    Atom *stco = trak->atomByName("stco");
    assert(stco);
    stco->content.resize(4 + //version
                         4 + //number of entries
                         4*offsets.size());
    stco->writeInt(offsets.size(), 4);
    for(unsigned int i = 0; i < offsets.size(); i++)
        stco->writeInt(offsets[i], 8 + 4*i);
}


