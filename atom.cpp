#include "AP_AtomDefinitions.h"
#include "atom.h"
#include "file.h"

#include <string>
#include <map>
#include <iostream>
#include <string.h>

#include <assert.h>

using namespace std;

Atom::Atom() {
	memset(name_, 0, 5);
}

Atom::~Atom() {
	for(unsigned int i = 0; i < children_.size(); i++)
		delete children_[i];
}

void Atom::parseHeader(FileRead &file) {
	start_ = file.pos();
	length_ = file.readInt();
	file.readChar(name_, 4);
	if(length_ == 1) {
		length_ = file.readInt64() - 8;
		start_ += 8;
	}
	else if(length_ == 0)
		length_ = file.length() - start_;

	logg(VV, "start_ = ", start_, '\n');
	logg(VV, "length_ = ", length_, '\n');
	logg(VV, "name_ = ", name_, '\n');
	logg(VV, '\n');

	if (length_ < 0) throw string("invalid atom length: ")+to_string(length_);
	if (start_ < 0) throw string("invalid atom start: ")+to_string(start_);  //  this is impossible?
	for (int i=0; i < 4; i++) if (!isalnum(name_[i])) throw string("invalid atom name: ")+name_;
}

void Atom::parse(FileRead &file) {
	parseHeader(file);

	if(isParent(name_) && name_ != string("udta")) { //user data atom is dangerous... i should actually skip all
		while(file.pos() < start_ + length_) {
			Atom *atom = new Atom;
			atom->parse(file);
			children_.push_back(atom);
		}
		assert(file.pos() == start_ + length_);

	}
	else if (name_ == string("mdat")) {
		int64_t content_size = length_ - 8;
		file.seekSafe(file.pos() + content_size);
	}
	else {
		content_ = file.read(length_ -8); //lenght includes header
		if(content_.size() < to_uint(length_ - 8))
			throw string("Failed reading atom content: ") + name_;
		logg(VV, '\n');  // align verbose buffer read message
	}
}

bool isValidAtomName(const uchar* buff) {
	if (!isdigit(*buff) && !islower(*buff)) return false;
	for(int i = 0; i < 174; i++)
		if (strncmp((char*)buff, knownAtoms[i].known_atom_name, 4) == 0) {
			return true;
		}
	return false;
}

off64_t Atom::findNextAtomOff(FileRead& file, Atom* start_atom, bool skip_nested) {
	static bool did_msg = false;
	off64_t next_off = skip_nested || string("mdat") == start_atom->name_? start_atom->start_ + start_atom->length_ : -1;
	if (next_off >= file.length()) return file.length();
	if (next_off > start_atom->start_ && string("avcC") != start_atom->name_ && isValidAtomName(file.getPtrAt(next_off+4, 4)))
		return next_off;
	else {
		if (skip_nested && !did_msg) {
			logg(I, "searching start of mdat ... \n");
			did_msg = true;
		}
		for(off64_t off=start_atom->start_+8; off < file.length();) {
			const uchar* buff = file.getPtrAt(off, 4);
			if (off % (1<<16) == 0) outProgress(off, file.length());
			if (!isdigit(*buff) && !islower(*buff)) {off += 4; continue;}
			for (int i=3; i >= 0; --i)
				if (isValidAtomName(buff-i)) return off-4-i;
			off += 7;
		}
	}
	return file.length();
}

void Atom::findAtomNames(string& filename) {
	FileRead file;
	bool success = file.open(filename);
	if(!success) throw string("Could not open file: ") + filename;
	Atom atom;

	bool ignore_avc1 = 0;
	off64_t off = findNextAtomOff(file, &atom, false);
	while (off < file.length()) {
		const uchar* buff = file.getPtrAt(off, 4);
		uint length = swap32(*(uint*)buff);
		memcpy(atom.name_, buff+4, 4);
		string name = string(atom.name_);
		atom.length_ = length;
		atom.start_ = off;

		if (name == "ftyp") ignore_avc1 = 1;
		if (!ignore_avc1 || name != "avc1") {
			printf("%zd: %.*s (%u)", off, 4, atom.name_, length);
			off64_t next_off = off + length;
			if (name == "avcC") printf(" <-- skipped\n");
			else if (next_off < file.length() && !isValidAtomName(file.getPtrAt(next_off+4, 4)))
				printf(" <-- invalid length\n");
			else
				printf("\n");
		}
		off = findNextAtomOff(file, &atom, false);
	}
}

void Atom::write(FileWrite &file) {
	//1 write length
	int start = file.pos();

	file.writeInt(length_);
	file.writeChar(name_, 4);
	if(content_.size())
		file.write(content_);
	for(unsigned int i = 0; i < children_.size(); i++)
		children_[i]->write(file);
	int end = file.pos();
	assert(end - start == length_);
}

void Atom::print(int offset) {
	string indent(offset, ' ');

	cout << string(offset, '-') << name_;
	if (g_atom_names.count(name_))
		cout << " \"" << g_atom_names.at(name_) << "\"";
	cout << " [" << start_ << ", " << length_ << "]\n";
	if(name_ == string("mvhd") || name_ == string("mdhd")) {
		for(int i = 0; i < offset; i++)
			cout << " ";
		//timescale: time units per second
		//duration: in time units
		cout << indent << " Timescale: " << readInt(12) << " Duration: " << readInt(16) << endl;

	} else if(name_ == string("tkhd")) {
		for(int i = 0; i < offset; i++)
			cout << " ";
		//track id:
		//duration
		cout << indent << " Trak: " << readInt(12) << " Duration: "  << readInt(20) << endl;

	} else if(name_ == string("hdlr")) {
		char type[5];
		readChar(type, 8, 4);
		cout << indent << " Type: " << type << endl;

	} else if(name_ == string("dref")) {
		cout << indent << " Entries: " << readInt(4) << endl;

	} else if(name_ == string("stsd")) { //sample description: (which codec...)
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

	} else if(name_ == string("stts")) { //run length compressed duration of samples
		//lets just read the first entry
		int entries = readInt(4);
		cout << indent << " Entries: " << entries << endl;
		for(int i = 0; i < entries && i < 30; i++)
			cout << indent << " samples: " << readInt(8 + 8*i) << " for: " << readInt(12 + 8*i) << endl;

	} else if(name_ == string("stss")) { //sync sample: (keyframes)
		//lets just read the first entry
		int entries = readInt(4);
		cout << indent << " Entries: " << entries << endl;
		for(int i = 0; i < entries && i < 10; i++)
			cout << indent << " Keyframe: " << readInt(8 + 4*i) << endl;


	} else if(name_ == string("stsc")) { //samples to chucnk:
		//lets just read the first entry
		int entries = readInt(4);
		cout << indent << " Entries: " << entries << endl;
		for(int i = 0; i < entries && i < 10; i++)
			cout << indent << " chunk: " << readInt(8 + 12*i) << " nsamples: " << readInt(12 + 12*i) << " id: " << readInt(16 + 12*i) << endl;

	} else if(name_ == string("stsz")) { //sample size atoms
		int entries = readInt(8);
		int sample_size = readInt(4);
		cout << indent << " Sample size: " << sample_size << " Entries: " << entries << endl;
		if(sample_size == 0) {
			for(int i = 0; i < entries && i < 10; i++)
				cout << indent << " Size " << readInt(12 + i*4) << endl;
		}

	} else if(name_ == string("stco")) { //sample chunk offset atoms
		int entries = readInt(4);
		cout << indent << " Entries: " << entries << endl;
		for(int i = 0; i < entries && i < 10; i++)
			cout << indent << " chunk: " << readInt(8 + i*4) << endl;

	} else if(name_ == string("co64")) {
		int entries = readInt(4);
		cout << indent << " Entries: " << entries << endl;
		for(int i = 0; i < entries && i < 10; i++)
			cout << indent << " chunk: " << readInt(12 + i*8) << endl;

	}

	for(unsigned int i = 0; i < children_.size(); i++)
		children_[i]->print(offset+1);
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
	for(unsigned int i = 0; i < children_.size(); i++) {
		if(children_[i]->name_ == name)
			atoms.push_back(children_[i]);
		vector<Atom *> a = children_[i]->atomsByName(name);
		atoms.insert(atoms.end(), a.begin(), a.end());
	}
	return atoms;
}
Atom *Atom::atomByName(std::string name) {
	for(unsigned int i = 0; i < children_.size(); i++) {
		if(children_[i]->name_ == name)
			return children_[i];
		Atom *a = children_[i]->atomByName(name);
		if(a) return a;
	}
	return NULL;
}

void Atom::replace(Atom *original, Atom *replacement) {
	for(unsigned int i = 0; i < children_.size(); i++) {
		if(children_[i] == original) {
			children_[i] = replacement;
			return;
		}
	}
	throw "Atom not found";
}

void Atom::prune(string name) {
	if(!children_.size()) return;

	length_ = 8;

	vector<Atom *>::iterator it = children_.begin();
	while(it != children_.end()) {
		Atom *child = *it;
		if(name == child->name_) {
			delete child;
			it = children_.erase(it);
		} else {
			child->prune(name);
			length_ += child->length_;
			++it;
		}
	}
}

void Atom::updateLength() {
	length_ = 8;
	length_ += content_.size();
//	assert(name_ != string("mdat"));

	for(unsigned int i = 0; i < children_.size(); i++) {
		Atom *child = children_[i];
		child->updateLength();
		length_ += child->length_;
	}
}

int64_t Atom::readInt64(int64_t offset) {
	return swap64(*(int64_t *)&(content_[offset]));
}

uint Atom::readInt(int64_t offset) {
	return swap32(*(uint *)&(content_[offset]));
}

void Atom::writeInt64(int64_t value, uint64_t offset) {
	assert(content_.size() >= offset + 8);
	*(int64_t *)&(content_[offset]) = swap64(value);
}

void Atom::writeInt(int value, uint64_t offset) {
	assert(content_.size() >= offset + 4);
	*(int *)&(content_[offset]) = swap32(value);
}

void Atom::readChar(char *str, int64_t offset, int64_t length) {
	for(int i = 0; i < length; i++)
		str[i] = content_[offset + i];

	str[length] = 0;
}


//WriteAtom::WriteAtom(FileRead& file): buffer_(NULL), buffer_begin_(0), file_read_(file) {
BufferedAtom::BufferedAtom(FileRead& file): file_read_(file) {
}

BufferedAtom::~BufferedAtom() {
}

const uchar *BufferedAtom::getFragment(int64_t offset, int64_t size) {
	if(offset < 0)
		throw "Offset set before beginning of file";
	if(offset + size > file_end_ - file_begin_)
		throw "Out of Range";
	file_read_.seek(file_begin_ + offset);
	return file_read_.getPtr(size);
}

void BufferedAtom::updateLength() {
	length_ = 8;
	length_ += file_end_ - file_begin_;

	for(unsigned int i = 0; i < children_.size(); i++) {
		Atom *child = children_[i];
		child->updateLength();
		length_ += child->length_;
	}
}

uint BufferedAtom::readInt(int64_t offset) {
	file_read_.seek(file_begin_ + offset); // not needed in currently
	return *(uint*) file_read_.getPtr(sizeof(int));
}

void BufferedAtom::write(FileWrite &output) {
	//1 write length
	off64_t start = output.pos();

	output.writeInt(length_);
	output.writeChar(name_, 4);
	off64_t offset = file_begin_;
	file_read_.seek(file_begin_);
	int loop_cnt = 0;
	while(offset < file_end_) {
		if (name_ == string("mdat") && g_log_mode == I && loop_cnt++ >= 10) {
			outProgress(offset, file_end_);
			loop_cnt = 0;
		}
		int toread = file_read_.buf_size_;
		if(toread + offset > file_end_)
			toread = file_end_ - offset;
		auto buff = file_read_.getPtr2(toread);
		offset += toread;
		output.writeChar(buff, toread);
	}
	for(unsigned int i = 0; i < children_.size(); i++)
		children_[i]->write(output);
	off64_t end = output.pos();
	assert(end - start == length_);
}
