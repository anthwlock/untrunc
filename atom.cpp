#include "AP_AtomDefinitions.h"
#include "atom.h"
#include "file.h"

#include <string>
#include <map>
#include <iostream>

#include <assert.h>
#include <endian.h>

using namespace std;


Atom::~Atom() {
    for(unsigned int i = 0; i < children.size(); i++)
        delete children[i];
}

void Atom::parseHeader(File &file) {
    start = file.pos();
    length = file.readInt();
    file.readChar(name, 4);

    if(length == 1) {
        length = file.readInt64() - 8;
        start += 8;
    } else if(length == 0) {
        length = file.length() - start;
    }
}

void Atom::parse(File &file) {
    parseHeader(file);

    if(isParent(name) && name != string("udta")) { //user data atom is dangerous... i should actually skip all
        while(file.pos() < start + length) {
            Atom *atom = new Atom;
            atom->parse(file);
            children.push_back(atom);
        }
        assert(file.pos() == start + length);

    } else {
        content = file.read(length -8); //lenght includes header
        if(content.size() < length -8)
            throw string("Failed reading atom content: ") + name;
    }
}

void Atom::write(File &file) {
    //1 write length
    int start = file.pos();

    file.writeInt(length);
    file.writeChar(name, 4);
    if(content.size())
        file.write(content);
    for(unsigned int i = 0; i < children.size(); i++)
        children[i]->write(file);
    int end = file.pos();
    assert(end - start == length);
}

void Atom::print(int offset) {
    string indent(offset, ' ');

    cout << string(offset, '-') << name << " [" << start << ", " << length << "]\n";
    if(name == string("mvhd") || name == string("mdhd")) {
        for(int i = 0; i < offset; i++)
            cout << " ";
        //timescale: time units per second
        //duration: in time units
        cout << indent << " Timescale: " << readInt(12) << " Duration: " << readInt(16) << endl;

    } else if(name == string("tkhd")) {
        for(int i = 0; i < offset; i++)
            cout << " ";
        //track id:
        //duration
        cout << indent << " Trak: " << readInt(12) << " Duration: "  << readInt(20) << endl;

    } else if(name == string("hdlr")) {
        char type[5];
        readChar(type, 8, 4);
        cout << indent << " Type: " << type << endl;

    } else if(name == string("dref")) {
        cout << indent << " Entries: " << readInt(4) << endl;

    } else if(name == string("stsd")) { //sample description: (which codec...)
        //lets just read the first entry
        char type[5];
        readChar(type, 12, 4);
        //4 bytes zero
        //4 bytes reference index (see stsc)
        //additional fields
        //video:
        //4 bytes zero
        ///avcC: //see ISO 14496  5.2.4.1.1.
        //01 -> version
        //4d -> profile
        //00 -> compatibility
        //28 -> level code
        //ff ->  6 bit reserved as 1  + 2 bit as nal length -1  so this is 4.
        //E1 -> 3 bit as 1 + 5 for SPS (so 1)
        //00 09 -> length of sequence parameter set
        //27 4D 00 28 F4 02 80 2D C8  -> sequence parameter set
        //01 -> number of picture parameter set
        //00 04 -> length of picture parameter set
        //28 EE 16 20 -> picture parameter set. (28 ee 04 62),  (28 ee 1e 20)

        cout << indent << " Entries: " << readInt(4) << " codec: " << type << endl;

    } else if(name == string("stts")) { //run length compressed duration of samples
        //lets just read the first entry
        int entries = readInt(4);
        cout << indent << " Entries: " << entries << endl;
		for(int i = 0; i < entries && i < 30; i++)
            cout << indent << " samples: " << readInt(8 + 8*i) << " for: " << readInt(12 + 8*i) << endl;

    } else if(name == string("stss")) { //sync sample: (keyframes)
        //lets just read the first entry
        int entries = readInt(4);
        cout << indent << " Entries: " << entries << endl;
        for(int i = 0; i < entries && i < 10; i++)
            cout << indent << " Keyframe: " << readInt(8 + 4*i) << endl;


    } else if(name == string("stsc")) { //samples to chucnk:
        //lets just read the first entry
        int entries = readInt(4);
        cout << indent << " Entries: " << entries << endl;
        for(int i = 0; i < entries && i < 10; i++)
            cout << indent << " chunk: " << readInt(8 + 12*i) << " nsamples: " << readInt(12 + 12*i) << " id: " << readInt(16 + 12*i) << endl;

    } else if(name == string("stsz")) { //sample size atoms
        int entries = readInt(8);
        int sample_size = readInt(4);
        cout << indent << " Sample size: " << sample_size << " Entries: " << entries << endl;
        if(sample_size == 0) {
            for(int i = 0; i < entries && i < 10; i++)
                cout << indent << " Size " << readInt(12 + i*4) << endl;
        }

    } else if(name == string("stco")) { //sample chunk offset atoms
        int entries = readInt(4);
        cout << indent << " Entries: " << entries << endl;
        for(int i = 0; i < entries && i < 10; i++)
            cout << indent << " chunk: " << readInt(8 + i*4) << endl;

    } else if(name == string("co64")) {
        int entries = readInt(4);
        cout << indent << " Entries: " << entries << endl;
        for(int i = 0; i < entries && i < 10; i++)
            cout << indent << " chunk: " << readInt(12 + i*8) << endl;

    }

    for(unsigned int i = 0; i < children.size(); i++)
        children[i]->print(offset+1);
}

AtomDefinition definition(char *id) {
    map<string, AtomDefinition> def;
    if(def.size() == 0) {
        for(int i = 0; i < 174; i++)
            def[knownAtoms[i].known_atom_name] = knownAtoms[i];
    }
    if(!def.count(id)) {
       //return a fake definition
        return def["<()>"];
    }
    return def[id];
}

bool Atom::isParent(char *id) {
    AtomDefinition def = definition(id);
    return def.container_state == PARENT_ATOM;// || def.container_state == DUAL_STATE_ATOM;
}

bool Atom::isDual(char *id) {
    AtomDefinition def = definition(id);
    return def.container_state == DUAL_STATE_ATOM;
}

bool Atom::isVersioned(char *id) {
    AtomDefinition def = definition(id);
    return def.box_type == VERSIONED_ATOM;
}

vector<Atom *> Atom::atomsByName(string name) {
    vector<Atom *> atoms;
    for(unsigned int i = 0; i < children.size(); i++) {
        if(children[i]->name == name)
            atoms.push_back(children[i]);
        vector<Atom *> a = children[i]->atomsByName(name);
        atoms.insert(atoms.end(), a.begin(), a.end());
    }
    return atoms;
}
Atom *Atom::atomByName(std::string name) {
    for(unsigned int i = 0; i < children.size(); i++) {
        if(children[i]->name == name)
            return children[i];
        Atom *a = children[i]->atomByName(name);
        if(a) return a;
    }
    return NULL;
}

void Atom::replace(Atom *original, Atom *replacement) {
    for(unsigned int i = 0; i < children.size(); i++) {
        if(children[i] == original) {
            children[i] = replacement;
            return;
        }
    }
    throw "Atom not found";
}

void Atom::prune(string name) {
    if(!children.size()) return;

    length = 8;

    vector<Atom *>::iterator it = children.begin();
    while(it != children.end()) {
        Atom *child = *it;
        if(name == child->name) {
            delete child;
            it = children.erase(it);
        } else {
            child->prune(name);
            length += child->length;
            ++it;
        }
    }
}

void Atom::updateLength() {
    length = 8;
    length += content.size();

    for(unsigned int i = 0; i < children.size(); i++) {
        Atom *child = children[i];
        child->updateLength();
        length += child->length;
    }
}

int Atom::readInt(int64_t offset) {
	return swap32(*(int *)&(content[offset]));
}

void Atom::writeInt(int value, int64_t offset) {
    assert(content.size() >= offset + 4);
	*(int *)&(content[offset]) = swap32(value);
}

void Atom::readChar(char *str, int64_t offset, int64_t length) {
    for(int i = 0; i < length; i++)
        str[i] = content[offset + i];

    str[length] = 0;
}


BufferedAtom::BufferedAtom(string filename): buffer(NULL) {
    if(!file.open(filename))
        throw "Could not open file.";
}

BufferedAtom::~BufferedAtom() {
    if(buffer) delete []buffer;
}

unsigned char *BufferedAtom::getFragment(int64_t offset, int64_t size) {
    if(offset < 0)
        throw "Offset set before beginning of buffer";
    if(offset + size > file_end - file_begin)
        throw "Out of buffer";

    if(buffer == NULL) {

        buffer_begin = offset;
        buffer_end = offset + 2*size;
        if(buffer_end + file_begin > file_end)
            buffer_end = file_end - file_begin;
        buffer = new unsigned char[buffer_end - buffer_begin];
        file.seek(file_begin + buffer_begin);
        file.readChar((char *)buffer, buffer_end - buffer_begin);
        return buffer;
    }
    if(buffer_begin >= offset && buffer_end >= offset + size)
        return buffer + (offset - buffer_begin);

    //reallocate and reread
    delete []buffer;
    buffer = NULL;
    return getFragment(offset, size);
}

void BufferedAtom::updateLength() {
    length = 8;
    length += file_end - file_begin;

    for(unsigned int i = 0; i < children.size(); i++) {
        Atom *child = children[i];
        child->updateLength();
        length += child->length;
    }
}

int BufferedAtom::readInt(int64_t offset) {
    if(!buffer || offset < buffer_begin || offset > (buffer_end - 4)) {
        buffer = getFragment(offset, 1<<16);
    }
    return *(int *)(buffer + offset - buffer_begin);
}

void BufferedAtom::write(File &output) {
    //1 write length
    int start = output.pos();

    output.writeInt(length);
    output.writeChar(name, 4);
    char buff[1<<20];
    int offset = file_begin;
    file.seek(file_begin);
    while(offset < file_end) {
        int toread = 1<<20;
        if(toread + offset > file_end)
            toread = file_end - offset;
        file.readChar(buff, toread);
        offset += toread;
        output.writeChar(buff,toread);
    }
    for(unsigned int i = 0; i < children.size(); i++)
        children[i]->write(output);
    int end = output.pos();
    assert(end - start == length);
}
