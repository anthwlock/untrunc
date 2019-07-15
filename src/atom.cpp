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
	name_.resize(4);
}

Atom::~Atom() {
	for (uint i=0; i < children_.size(); i++)
		delete children_[i];
}



void Atom::parseHeader(FileRead &file) {
	start_ = file.pos();
	length_ = file.readInt();
	name_ = file.getString(4);
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

	if (length_ < 0) {
		logg(W, "negative atom length: ", length_, "\n");
		length_ = 8;
	}
	if (start_ < 0) throw ss("atom start: ", start_);  //  this is impossible?
	for (int i=0; i < 4; i++) if (!isalnum(name_[i])) throw ss("invalid atom name: '", name_, "'");
}

bool isValidAtomName(const uchar* buff) {
	if (!isdigit(*buff) && !islower(*buff)) return false;
	for(int i = 0; i < 174; i++)
		if (strncmp((char*)buff, knownAtoms[i].known_atom_name, 4) == 0) {
			return true;
		}
	return false;
}

bool isPointingAtAtom(FileRead& file) {
	return file.atEnd() || isValidAtomName(file.getPtr(8)+4);
}

void Atom::parse(FileRead& file) {
	parseHeader(file);

	if(isParent(name_) && name_ != "udta") { //user data atom is dangerous... i should actually skip all
		while(file.pos() < start_ + length_) {
			Atom *atom = new Atom;
			atom->parse(file);
			children_.push_back(atom);
		}
		assert(file.pos() == start_ + length_);

	}
	else if (name_ == "mdat") {
		int64_t content_size = length_ - 8;
		file.seekSafe(file.pos() + content_size);
		if (!isPointingAtAtom(file))
			throw "bad 'mdat' length";
	}
	else {
		content_ = file.read(length_ -8); //lenght includes header
		if(content_.size() < to_uint(length_ - 8))
			throw ss("Failed reading atom content: ", name_);
		logg(VV, '\n');  // align verbose buffer read message
	}
}

off64_t Atom::findNextAtomOff(FileRead& file, const Atom* start_atom, bool searching_mdat) {
	static int show_mdat_msg_in = 2;  // first start_atom is not initialized
	off64_t next_off = start_atom->start_ + start_atom->length_;
	bool skip_nested = ((searching_mdat && start_atom->name_ != "avcC" ) || start_atom->name_ == "mdat") &&
	                   next_off > start_atom->start_;

	if (skip_nested) {
		if (next_off >= file.length()) return file.length();
		if (isValidAtomName(file.getPtrAt(next_off+4, 4))) return next_off;
	}

	if (searching_mdat && !--show_mdat_msg_in) logg(I, "'", file.filename_, "' has invalid atom lenghts, see '-f'\n");

	for(off64_t off=start_atom->start_+8; off < file.length();) {
		auto buff = file.getPtrAt(off, 4);
		if (g_log_mode == LogMode::I && off % (1<<16) == 0) outProgress(off, file.length());
		if (!isdigit(*buff) && !islower(*buff)) {off += 4; continue;}
		for (int i=3; i >= 0; --i)
			if (isValidAtomName(buff-i)) return off-4-i;
		off += 7;
	}
	return file.length();
}

void Atom::findAtomNames(string& filename) {
	FileRead file(filename);
	Atom atom;

	bool ignore_avc1 = 0;
	off64_t off = findNextAtomOff(file, &atom);
	while (off < file.length()) {
		const uchar* buff = file.getPtrAt(off, 4);
		uint length = swap32(*(uint*)buff);
		atom.name_ = string((char*)buff+4, 4);
		atom.length_ = length;
		atom.start_ = off;

		if (atom.name_ == "ftyp") ignore_avc1 = 1;
		if (!ignore_avc1 || atom.name_ != "avc1") {
			cout << ss(off, ": ", atom.name_, " (", length, ")");
			off64_t next_off = off + length;
			if (atom.name_ == "avcC") cout << " <-- skipped\n";
			else if (next_off < file.length() && !isValidAtomName(file.getPtrAt(next_off+4, 4)))
				cout << " <-- invalid length\n";
			else
				cout << "\n";
		}
		off = findNextAtomOff(file, &atom);
	}
}

void Atom::write(FileWrite &file) {
	//1 write length
	int start = file.pos();

	file.writeInt(length_);
	file.writeChar(name_.data(), 4);
	if(content_.size())
		file.write(content_);
	for (uint i=0; i < children_.size(); i++)
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
	if(name_ == "mvhd" || name_ == "mdhd") {
		for(int i = 0; i < offset; i++)
			cout << " ";
		//timescale: time units per second
		//duration: in time units
		cout << indent << " Timescale: " << readInt(12) << " Duration: " << readInt(16) << endl;

	} else if(name_ == "tkhd") {
		for(int i = 0; i < offset; i++)
			cout << " ";
		//track id:
		//duration
		cout << indent << " Trak: " << readInt(12) << " Duration: "  << readInt(20) << endl;

	} else if(name_ == "hdlr") {
		auto type = getString(8, 4);
		cout << indent << " Type: " << type << endl;

	} else if(name_ == "dref") {
		cout << indent << " Entries: " << readInt(4) << endl;

	} else if(name_ == "stsd") { //sample description: (which codec...)
		//lets just read the first entry
		auto type = getString(12, 4);
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

	} else if(name_ == "stts") { //run length compressed duration of samples
		//lets just read the first entry
		int entries = readInt(4);
		cout << indent << " Entries: " << entries << endl;
		for(int i = 0; i < entries && i < 30; i++)
			cout << indent << " samples: " << readInt(8 + 8*i) << " for: " << readInt(12 + 8*i) << endl;

	} else if(name_ == "stss") { //sync sample: (keyframes)
		//lets just read the first entry
		int entries = readInt(4);
		cout << indent << " Entries: " << entries << endl;
		for(int i = 0; i < entries && i < 10; i++)
			cout << indent << " Keyframe: " << readInt(8 + 4*i) << endl;


	} else if(name_ == "stsc") { //samples to chucnk:
		//lets just read the first entry
		int entries = readInt(4);
		cout << indent << " Entries: " << entries << endl;
		for(int i = 0; i < entries && i < 10; i++)
			cout << indent << " chunk: " << readInt(8 + 12*i) << " nsamples: " << readInt(12 + 12*i) << " id: " << readInt(16 + 12*i) << endl;

	} else if(name_ == "stsz") { //sample size atoms
		int entries = readInt(8);
		int sample_size = readInt(4);
		cout << indent << " Sample size: " << sample_size << " Entries: " << entries << endl;
		if(sample_size == 0) {
			for(int i = 0; i < entries && i < 10; i++)
				cout << indent << " Size " << readInt(12 + i*4) << endl;
		}

	} else if(name_ == "stco") { //sample chunk offset atoms
		int entries = readInt(4);
		cout << indent << " Entries: " << entries << endl;
		for(int i = 0; i < entries && i < 10; i++)
			cout << indent << " chunk: " << readInt(8 + i*4) << endl;

	} else if(name_ == "co64") {
		int entries = readInt(4);
		cout << indent << " Entries: " << entries << endl;
		for(int i = 0; i < entries && i < 10; i++)
			cout << indent << " chunk: " << readInt(12 + i*8) << endl;

	}

	for (uint i=0; i < children_.size(); i++)
		children_[i]->print(offset+1);
}

AtomDefinition definition(const string& id) {
	static map<string, AtomDefinition> def;
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

bool Atom::isParent(const string& id) {
	AtomDefinition def = definition(id);
	return def.container_state == PARENT_ATOM;// || def.container_state == DUAL_STATE_ATOM;
}

bool Atom::isDual(const string& id) {
	AtomDefinition def = definition(id);
	return def.container_state == DUAL_STATE_ATOM;
}

bool Atom::isVersioned(const string& id) {
	AtomDefinition def = definition(id);
	return def.box_type == VERSIONED_ATOM;
}

vector<Atom *> Atom::atomsByName(const string& name) {
	vector<Atom *> atoms;
	for (uint i=0; i < children_.size(); i++) {
		if(children_[i]->name_ == name)
			atoms.push_back(children_[i]);
		vector<Atom *> a = children_[i]->atomsByName(name);
		atoms.insert(atoms.end(), a.begin(), a.end());
	}
	return atoms;
}
Atom *Atom::atomByName(const string& name) {
	for (uint i=0; i < children_.size(); i++) {
		if(children_[i]->name_ == name)
			return children_[i];
		Atom *a = children_[i]->atomByName(name);
		if(a) return a;
	}
	return NULL;
}

void Atom::replace(Atom *original, Atom *replacement) {
	for (uint i=0; i < children_.size(); i++) {
		if(children_[i] == original) {
			children_[i] = replacement;
			return;
		}
	}
	throw "Atom not found";
}

void Atom::prune(const string& name) {
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

	for (uint i=0; i < children_.size(); i++) {
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

string Atom::getString(int64_t offset, int64_t length) {
	return string((char*)&content_[offset], length);
}


BufferedAtom::BufferedAtom(FileRead& file): file_read_(file) {}

const uchar *BufferedAtom::getFragment(int64_t offset, int64_t size) {
	if(offset < 0)
		throw "Offset set before beginning of file";
	if(offset + size > file_end_ - file_begin_)
		throw "Out of Range";
	file_read_.seek(file_begin_ + offset);  // cheap seek
	return file_read_.getPtr(size);
}

void BufferedAtom::updateLength() {
	length_ = 8;
	length_ += file_end_ - file_begin_;

	for (uint i=0; i < children_.size(); i++) {
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
	output.writeChar(name_.data(), 4);
	off64_t offset = file_begin_;
	file_read_.seek(file_begin_);
	int loop_cnt = 0;
	while(offset < file_end_) {
		if (name_ == "mdat" && g_log_mode == I && loop_cnt++ >= 10) {
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
	for (uint i=0; i < children_.size(); i++)
		children_[i]->write(output);
	off64_t end = output.pos();
	assert(end - start == length_);
}
