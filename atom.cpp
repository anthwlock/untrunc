//==================================================================//
/*                                                                  *
	Untrunc - atom.cpp

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
 *                                                                  */
//==================================================================//

#include "atom.h"

#include <cassert>
#include <iostream>
#include <map>
#include <stdexcept>

#include "AP_AtomDefinitions.h"


using std::cout;
using std::endl;
using std::length_error;
using std::map;
using std::string;
using std::vector;


Atom::~Atom() {
	for (unsigned int i = 0; i < children_.size(); i++)
		delete children_[i];
}

void Atom::parseHeader(const FileRead& file_rd) {
	FileRead& file = const_cast<FileRead&>(file_rd);
	start_ = file.pos();
	logg(V, "start_ = ", start_, '\n');
	length_ = file.readInt();
	logg(V, "length_ = ", length_, '\n');
	if (length_ < 0)
		throw length_error("length of atom < 0");
	file.readChar(name_, 4);
	logg(V, "name_ = ", name_, '\n');

	if (length_ == 1) {
		length_ = file.readInt64() - 8;
		start_ += 8;
	} else if (length_ == 0) {
		length_ = file.length() - start_;
	}
}

void Atom::parse(const FileRead& file_rd) {
	FileRead& file = const_cast<FileRead&>(file_rd);
	logg(V, '\n');
	parseHeader(file);

	if (isParent(name_) &&
		name_ != string("udta")) {
		// User Data atom is dangerous... I should actually skip all.
		while (file.pos() < start_ + length_) {
			Atom* atom = new Atom;
			atom->parse(file);
			children_.push_back(atom);
		}
		assert(file.pos() == start_ + length_);

	} else {
		content_ = file.read(length_ - 8);  // Lenght includes header.
		if (content_.size() < length_ - 8)
			throw string("Failed reading atom content: ") + name_;
	}
}

void Atom::write(FileWrite* file) {
	if (!file) return;
	// 1 write length.
	int start = file->pos();

	file->writeInt(length_);
	file->writeChar(name_, 4);
	if (content_.size())
		file->write(content_);
	for (unsigned int i = 0; i < children_.size(); i++)
		children_[i]->write(file);
	int end = file->pos();
	//cout << "end = " << end << '\n';
	//cout << "length_ = " << length_ << '\n';
	assert(end - start == length_);
}

void Atom::print(int indentation) {
	string indent(indentation, ' ');

	cout << string(indentation, '-') << name_;
	if (g_atom_names.count(name_))
		cout << " \"" << g_atom_names.at(name_) << "\"";
	cout << " [" << start_ << ", " << length_ << "]\n";
	if (name_ == string("mvhd") || name_ == string("mdhd")) {
		for (int i = 0; i < indentation; i++)
			cout << " ";
		// Timescale: time units per second
		// Duration: in time units
		cout << indent << " Timescale: " << readInt(12)
			 << " Duration: " << readInt(16) << endl;

	} else if (name_ == string("tkhd")) {
		for (int i = 0; i < indentation; i++)
			cout << " ";
		// Track id:
		// Duration:
		cout << indent << " Trak: " << readInt(12)
			 << " Duration: " << readInt(20) << endl;

	} else if (name_ == string("hdlr")) {
		char type[5];
		readChar(type, 8, 4);
		cout << indent << " Type: " << type << endl;

	} else if (name_ == string("dref")) {
		cout << indent << " Entries: " << readInt(4) << endl;

	} else if (name_ == string("stsd")) {  // Sample description (which codec).
		// Lets just read the first entry.
		char type[5];
		readChar(type, 12, 4);
		// 4 bytes zero
		// 4 bytes reference index (see stsc)
		// additional fields
		// video:
		// 4 bytes zero
		/// avcC: //see ISO 14496  5.2.4.1.1.
		// 01 -> version
		// 4d -> profile
		// 00 -> compatibility
		// 28 -> level code
		// ff ->  6 bit reserved as 1  + 2 bit as nal length -1  so this is 4.
		// E1 -> 3 bit as 1 + 5 for SPS (so 1)
		// 00 09 -> length of sequence parameter set
		// 27 4D 00 28 F4 02 80 2D C8  -> sequence parameter set
		// 01 -> number of picture parameter set
		// 00 04 -> length of picture parameter set
		// 28 EE 16 20 -> picture parameter set. (28 ee 04 62),  (28 ee 1e 20)

		cout << indent << " Entries: " << readInt(4)
			 << " codec: " << type << endl;

	} else if (name_ == string("stts")) {  // Run-length compressed
										   //  duration of samples.
		// Lets just read the first entry.
		int entries = readInt(4);
		cout << indent << " Entries: " << entries << endl;
		for (int i = 0; i < entries && i < 30; i++)
			cout << indent << " samples: " << readInt(8 + 8 * i)
				 << " for: " << readInt(12 + 8 * i) << endl;

	} else if (name_ == string("stss")) {  // Sync sample (keyframes).
		// Lets just read the first entry.
		int entries = readInt(4);
		cout << indent << " Entries: " << entries << endl;
		for (int i = 0; i < entries && i < 10; i++)
			cout << indent << " Keyframe: " << readInt(8 + 4 * i) << endl;


	} else if (name_ == string("stsc")) {  // Samples to chucnk.
		// Lets just read the first entry.
		int entries = readInt(4);
		cout << indent << " Entries: " << entries << endl;
		for (int i = 0; i < entries && i < 10; i++)
			cout << indent << " chunk: " << readInt(8 + 12 * i)
				 << " nsamples: " << readInt(12 + 12 * i)
				 << " id: " << readInt(16 + 12 * i) << endl;

	} else if (name_ == string("stsz")) {  // Sample size atoms.
		int entries = readInt(8);
		int sample_size = readInt(4);
		cout << indent << " Sample size: " << sample_size
			 << " Entries: " << entries << endl;
		if (sample_size == 0) {
			for (int i = 0; i < entries && i < 10; i++)
				cout << indent << " Size " << readInt(12 + i * 4) << endl;
		}

	} else if (name_ == string("stco")) {  // Sample chunk offset atoms.
		int entries = readInt(4);
		cout << indent << " Entries: " << entries << endl;
		for (int i = 0; i < entries && i < 10; i++)
			cout << indent << " chunk: " << readInt(8 + i * 4) << endl;

	} else if (name_ == string("co64")) {
		int entries = readInt(4);
		cout << indent << " Entries: " << entries << endl;
		for (int i = 0; i < entries && i < 10; i++)
			cout << indent << " chunk: " << readInt(12 + i * 8) << endl;
	}

	for (unsigned int i = 0; i < children_.size(); i++)
		children_[i]->print(indentation + 1);
}

AtomDefinition definition(const char* id) {
	map<string, AtomDefinition> def;
	if (def.size() == 0) {
		for (int i = 0; i < 174; i++)
			def[knownAtoms[i].known_atom_name] = knownAtoms[i];
	}
	if (!def.count(id)) {
		// Return a fake definition.
		return def["<()>"];
	}
	return def[id];
}

bool Atom::isParent(const char* id) {
	AtomDefinition def = definition(id);
	return def.container_state == PARENT_ATOM;
		//|| def.container_state == DUAL_STATE_ATOM;
}

bool Atom::isDual(const char* id) {
	AtomDefinition def = definition(id);
	return def.container_state == DUAL_STATE_ATOM;
}

bool Atom::isVersioned(const char* id) {
	AtomDefinition def = definition(id);
	return def.box_type == VERSIONED_ATOM;
}

vector<Atom*> Atom::atomsByName(const string& name) const {
	vector<Atom*> atoms;
	for (unsigned int i = 0; i < children_.size(); i++) {
		if (children_[i]->name_ == name)
			atoms.push_back(children_[i]);
		vector<Atom*> a = children_[i]->atomsByName(name);
		atoms.insert(atoms.end(), a.begin(), a.end());
	}
	return atoms;
}
Atom* Atom::atomByName(const std::string& name) const {
	for (unsigned int i = 0; i < children_.size(); i++) {
		if (children_[i]->name_ == name)
			return children_[i];
		Atom* a = children_[i]->atomByName(name);
		if (a)
			return a;
	}
	return NULL;
}

void Atom::replace(const Atom* original, Atom* replacement) {
	for (unsigned int i = 0; i < children_.size(); i++) {
		if (children_[i] == original) {
			children_[i] = replacement;
			return;
		}
	}
	throw "Atom not found";
}

void Atom::prune(const string& name) {
	if (!children_.size())
		return;

	length_ = 8;

	vector<Atom*>::iterator it = children_.begin();
	while (it != children_.end()) {
		Atom* child = *it;
		if (name == child->name_) {
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

	for (unsigned int i = 0; i < children_.size(); i++) {
		Atom* child = children_[i];
		child->updateLength();
		length_ += child->length_;
	}
}

int Atom::readInt(int64_t offset) {
	return swap32(*reinterpret_cast<int*>(&content_[offset]));
}

void Atom::writeInt(int value, int64_t offset) {
	assert(content_.size() >= offset + 4);
	*reinterpret_cast<int*>(&content_[offset]) = swap32(value);
}

void Atom::readChar(char* str, int64_t offset, int64_t length) {
	for (int i = 0; i < length; i++)
		str[i] = content_[offset + i];

	str[length] = 0;
}


WriteAtom::WriteAtom(const FileRead& file) : file_read_(const_cast<FileRead&>(file)) {}

WriteAtom::~WriteAtom() {}

const uchar* WriteAtom::getFragment(int64_t offset, int64_t size) {
	if (offset < 0)
		throw "Offset set before beginning of file";
	if (offset + size > file_end_ - file_begin_)
		throw "Out of Range";
	file_read_.seek(file_begin_ + offset);
	return file_read_.getPtr(size);
}

void WriteAtom::updateLength() {
	length_ = 8;
	length_ += file_end_ - file_begin_;

	for (unsigned int i = 0; i < children_.size(); i++) {
		Atom* child = children_[i];
		child->updateLength();
		length_ += child->length_;
	}
}

int WriteAtom::readInt(int64_t offset) {
	file_read_.seek(file_begin_ + offset);  // Not needed currently.
	return *reinterpret_cast<const int*>(file_read_.getPtr(sizeof(int)));
}

void WriteAtom::write(FileWrite* file) {
	if (!file) return;
	// 1 write length.
	int start = file->pos();

	file->writeInt(length_);
	file->writeChar(name_, 4);
	char buff[1 << 20];
	int offset = file_begin_;
	file_read_.seek(file_begin_);
	while (offset < file_end_) {
		int toread = 1 << 20;
		if (toread + offset > file_end_)
			toread = file_end_ - offset;
		file_read_.readChar(buff, toread);
		offset += toread;
		file->writeChar(buff, toread);
	}
	for (unsigned int i = 0; i < children_.size(); i++)
		children_[i]->write(file);
	int end = file->pos();
	assert(end - start == length_);
}


// vim:set ts=4 sw=4 sts=4 noet:
