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
#include <cstring>
#include <algorithm>
#include <iostream>
#include <iomanip>
#include <limits>
#include <map>
#include <utility>

#include "AP_AtomDefinitions.h"
#include "atom-names.h"


using std::cout;
using std::exchange;
using std::forward;
using std::map;
using std::memcpy;
using std::min;
using std::string;
using std::vector;


namespace {
// Atom definitions mappings.
AtomDefinition definition(uint32_t id) {
	static const AtomDefinition AtomDefUnknown = KnownAtoms[0];
	static map<uint32_t, AtomDefinition> defs;
	if (defs.empty()) {
		for (const auto& atom_def : KnownAtoms) {
#if 1
			// For each atom name include the last of multiple definitions.
			defs[name2Id(atom_def.known_atom_name)] = atom_def;
#else
			// For each atom name include only the first of multiple definitions.
			defs.insert(make_pair(name2Id(atom_def.known_atom_name), atom_def));
#endif
		}
	}

	const auto& it = defs.find(id);
	return (it != defs.end()) ? it->second : AtomDefUnknown;
}


// Full atom name mappings.
const char* atomFullName(uint32_t id) {
	if (id != 0) {
		const auto& it = AtomFullNames.find(id);
		if (it != AtomFullNames.end())
			return it->second;
	}
	return nullptr;
}

string atomCompleteName2(const char* name, const char* full_name) {
	string compl_name(name);
	return (!full_name) ? compl_name : compl_name + " '" + full_name + '\'';
}
}; // namespace


//
// Atom Class.
//
Atom::Atom(uint32_t id) : id_(id) {
	id2Name(name_, id);
}

Atom::Atom(const char* name) {
	if (name) {
		assert(std::strlen(name) >= 3);  // An atom name is 3 or 4 chars.
		id_ = name2Id(name);
		memcpy(name_, name, 4);
	}
}

Atom::~Atom() {
	for (auto& child : children_)
		delete exchange(child, nullptr);
}


// Atom parsing.
bool Atom::parseHeader(const FileRead& file_rd) {
	logg(VV, "Parse atom header.\n");
	if (!file_rd || file_rd.size() < 0) {
		throw string("Could not parse atom header from unopened file");
		return false;  // No header parsed.
	}
	FileRead& file = const_cast<FileRead&>(file_rd);
	off_t pos = file.pos();
	if (pos < 0) {
		file.seek(0L);
		pos = file.pos();
	}
	if (pos < 0) {
		throw string("Could not parse atom header from bad file");
		return false;  // No header parsed.
	}
	if (file.atEnd()) return false;  // No header parsed.

	file_atom_pos_ = pos;
	length_        = file.readUint32();
	length_zero_   = (length_ == 0);
	length_valid_  = true;
	file.readChar(name_, 4);
	id_            = name2Id(name_);

	size_t file_atom_size =
		(file_atom_pos_ < file.size())
		? static_cast<size_t>(file.size() - file_atom_pos_)
		: 0U;

	static_assert(std::numeric_limits<off_t>::digits <= 64,
				  "Implement larger atom length");
	unsigned int length_extens = 0;
	if (length_ == 0) {
		// Length is till the end of the file.
		length_ = file_atom_size;
	} else if (length_ < 8) {
		// One or more 64-bit length extensions.
		length_extens = length_;
		if (length_extens > 1) {
			logg(W, "Atom ", name_, " has too many length extensions (",
				    length_extens, ").\n");
		}
		uint64_t length_extensions[7] = {};
		for (unsigned int i = 0; i < length_extens; ++i) {
			length_extensions[i] = file.readUint64();
			if (length_extensions[i] != 0 && i + 1 < length_extens) {
				// We can only handle one length extension.
				length_valid_ = false;
				break;
			}
		}
		if (!length_valid_) {
			// ASSUME: The number of length extensions is wrong.
			logg(E, "Reducing atom ", name_, " length extensions (",
				    length_extens, " -> 1).\n");
			file.seek(file_atom_pos_ + 8 + 8);
			length_extens = 1;
		}
		length_ = length_extensions[length_extens - 1];

		// Untrunc header size is always kMinHeaderSize (8).
		length_        -= length_extens * kMinHeaderSize;
		file_atom_pos_ += length_extens * kMinHeaderSize;
	}

	file_content_pos_  = file.pos();

	unsigned int hdrsize = 8 + length_extens * 8;
	if (length_ < hdrsize) {
		length_valid_ = false;
		logg(E, "Atom ", name_, " length to small (",
			    length_, " < ", hdrsize, ").\n");
	}
	if (length_ > std::numeric_limits<off_t>::max()) {
		length_valid_ = false;
		logg(E, "Atom ", name_, " length exceeds limit (",
			    length_, " -> ", std::numeric_limits<off_t>::max(), ").\n");
		length_ = std::numeric_limits<off_t>::max();
	}
	if (length_ > file_atom_size) {
		length_valid_ = false;
		logg(E, "Atom ", name_, " length outside file (",
			    length_, " -> ", file_atom_size, ").\n");
		length_ = file_atom_size;
	}

	logg(VV, "name_              = ", name_, " '", fullName(),
		  "'\nid_                = 0x", std::hex, id_, std::dec,
		   "\nlength_            = ", length_,
		   "\nlength_zero_       = ", length_zero_,
		   "\nlength_valid_      = ", length_valid_,
		   "\nfile_atom_pos_     = ", file_atom_pos_,
		   "\nfile_content_pos_  = ", file_content_pos_,
		   '\n');
	return true;  // Got header data (possibly incorrect);
}

bool Atom::nextHeader(const FileRead& file_rd) {
	logg(VV, "Seek to next atom header.\n");
	if (!file_rd) {
		throw string("Could not parse atom header from unopened file");
		return false;
	}
	FileRead& file = const_cast<FileRead&>(file_rd);

	file.seek(file_atom_pos_ + length_);
	return !file.atEnd();
}

bool Atom::parse(const FileRead& file_rd) {
	logg(VV, "Parse atom.\n");
	if (!parseHeader(file_rd)) return false;
	FileRead& file = const_cast<FileRead&>(file_rd);

	// The udta 'User-Data' atom is dangerous... I should actually skip all.
	if (isParent() && id_ != name2Id("udta")) {
		while (static_cast<unsigned off_t>(file.pos()) < file_atom_pos_ + length_) {
			Atom* atom = new Atom;
			if (atom->parse(file)) {
				children_.push_back(atom);
			} else {
				delete atom;
				throw string("Failed reading content of atom: ") + completeName();
				return false;
			}
		}
		assert(file.pos() == static_cast<off_t>(file_atom_pos_ + length_));
	} else {
		content_ = file.read(length_ - 8);  // Lenght includes header.
		if (content_.size() < size_t(length_ - 8)) {
			throw string("Failed reading content of atom: ") + completeName();
			return false;
		}
	}
	return true;  // Atom has been parsed fully.
}

void Atom::writeHeader(FileWrite* file) {
	if (length_ > std::numeric_limits<uint32_t>::max()) {
		logg(E, "Length of atom ", name_, " too large (", length_, " > ",
			 std::numeric_limits<uint32_t>::max(), ").\n");
	}

	file->writeUint32(length_);
	file->writeChar(name_, 4);
}

void Atom::write(FileWrite* file) {
	if (!file) return;
	logg(VV, "Write atom ", name_, ".\n");
#ifndef NDEBUG
	off_t start = file->pos();
#endif

	// Write header.
	writeHeader(file);
	// Write content.
	if (!content_.empty()) {
		ssize_t len = file->write(content_);
		if (len < 0)
			logg(E, "File error while writing atom: ", completeName(), ".\n");
		else if (size_t(len) != content_.size())
			logg(E, "Incomplete written atom: ", completeName(), ".\n");
	}
	// Write children.
	for (const auto& child : children_)
		child->write(file);

#ifndef NDEBUG
	off_t end = file->pos();
	logg(VV, "Written length: ", (end - start), " start: ", start, " end: ", end,
		     "  Atom length: ", length_, '\n');
	assert(static_cast<unsigned off_t>(end - start) == length_);
#endif
}


void Atom::print(int indentation) const {
	if (indentation < 0) indentation = 0;
	const string indent1(indentation + 1 * kIndentStep, ' ');
	const string indent2(indentation + 2 * kIndentStep, ' ');

	string complete_name(completeName());
	if (complete_name.empty()) {
		complete_name =
			(file_atom_pos_ == -1 && !children_.empty())
			? "(root)"
			: (file_atom_pos_ == -1 && length_ == 0)
			? "(uninitialized)"
			: "(""????"")";  // Prevent trigraph.
	}
	cout << string(indentation, '-') << "Atom: " << complete_name;
	if (file_atom_pos_ != -1 || length_ != 0)
		cout << " [" << file_atom_pos_ << ": " << length_ << " byte]";
	cout << '\n';

	switch (id_) {
		case name2Id("mvhd"):
		case name2Id("mdhd"):
			// Timescale: time units per second
			// Duration:  in time units
			cout << indent1 << "Timescale: " << readUint32(12)
							<< " Duration: " << readUint32(16) << '\n';
			break;

		case name2Id("tkhd"): {
				// 4 vers+flags, 4 ctime, 4 mtime, 4 id, 4 rsrvd, 4 duration, ..
				// 4 vers+flags, 8 ctime, 8 mtime, 4 id, 4 rsrvd, 8 duration, ..
				// Duration:  in time units
				uint32_t vers_flags = readUint32(0);
				if ((vers_flags >> 24) == 0) {
					cout << indent1 << "Trak: "      << readUint32(12)
									<< ((vers_flags & 1) ? " (on)" : " (off)")
									<< " Duration: " << readUint32(20) << '\n';
				} else {
					cout << indent1 << "Trak: "      << readUint32(20)
									<< ((vers_flags & 1) ? " (on)" : " (off)")
									<< " Duration: " << readUint64(28) << '\n';
				}
				break;
			}

		case name2Id("hdlr"):
			// For meta.hdlr and moov.trak.mdia.hdlr,
			//   moov.trak.udta.meta.hdlr & moov.trak.mdia.udta.meta.hdlr atoms;
			// incomplete for moov.trak.mdia.minf.hdlr atom.
			cout << indent1 << "Type: " << completeName(readUint32(8)) << '\n';
			break;

		case name2Id("dref"):
			// For moov.trak.mdia.minf.dinf.dref atom;
			// not for moov.mdra.dref atom.
			cout << indent1 << "Entries: " << readUint32(4) << '\n';
			break;

		case name2Id("stsd"): {  // Sample description (which codec).
				//  4 bytes zero
				//  4 bytes reference index (see "stsc")
				// Additional fields
				// video:
				//  4 bytes zero
				// avcC:  // see: ISO 14496, par. 5.2.4.1.1.
				//  01    -> version
				//  4d    -> profile
				//  00    -> compatibility
				//  28    -> level code
				//  ff    -> 6 bit reserved as 1 + 2 bit as NAL length -1
				//           so this is 4
				//  E1    -> 3 bit as 1 + 5 for SPS (so 1)
				//  00 09 -> length of sequence parameter set
				//  27 4D 00 28 F4 02 80 2D C8 -> sequence parameter set
				//  01    -> number of picture parameter set
				//  00 04 -> length of picture parameter set
				//  28 EE 16 20 -> picture parameter set.
				//                 (28 ee 04 62), (28 ee 1e 20)
#if 0
				// Lets just read the first entry.
				cout << indent1 << "Entries: " << readUint32(4)
								<< " codec: "
								<< completeName(readUint32(12)) << '\n';
#else
				uint32_t entries = readUint32(4);
				cout << indent1 << "Entries: " << entries << '\n';
				int64_t ofs = 8;
				for (unsigned int i = 0; i < entries && i < 10; ++i) {
					cout << indent2 << "codec: "
									<< completeName(readUint32(ofs + 4)) << '\n';
					uint32_t entry_len = readUint32(ofs);
# if 0
					size_t dump_len = min(size_t(entry_len), contentSize(ofs));
					hexDump(content(ofs, dump_len), dump_len, ofs);
# endif
					ofs += entry_len;
					if ((entry_len == 0 && i + 1 != entries) ||
						uint64_t(ofs) > contentSize()) {
							logg(E, "Invalid length in entry: ", i,
								" of atom: ", completeName(), ".\n");
						break;
					}
					if (entry_len == 0) break;
				}
#endif  // 0
				break;
			}

		case name2Id("stts"): {  // Run-length compressed duration of samples.
				uint32_t entries = readUint32(4);
				cout << indent1 << "Entries: " << entries << '\n';
				for (unsigned int i = 0; i < entries && i < 30; ++i) {
					cout << indent2 << "samples: " << readUint32( 8 + 8 * i)
									<< " for: "    << readUint32(12 + 8 * i)
									<< '\n';
				}
				break;
			}

		case name2Id("stss"): {  // Sync sample (keyframes).
				uint32_t entries = readUint32(4);
				cout << indent1 << "Entries: " << entries << '\n';
				for (unsigned int i = 0; i < entries && i < 10; ++i) {
					cout << indent2 << "keyframe: " << readUint32(8 + 4 * i)
									<< '\n';
				}
				break;
			}

		case name2Id("stsc"): {  // Samples to chucnk.
				uint32_t entries = readUint32(4);
				cout << indent1 << "Entries: " << entries << '\n';
				for (unsigned int i = 0; i < entries && i < 10; ++i) {
					cout << indent2 << "chunk: "     << readUint32( 8 + 12 * i)
									<< " nsamples: " << readUint32(12 + 12 * i)
									<< " id: "       << readUint32(16 + 12 * i)
									<< '\n';
				}
				break;
			}

		case name2Id("stsz"): {  // Sample size atoms.
				uint32_t sample_size = readUint32(4);
				uint32_t entries     = readUint32(8);
				cout << indent1 << "Sample size: " << sample_size
								<< " Entries: "    << entries << '\n';
				if (sample_size == 0) {
					for (unsigned int i = 0; i < entries && i < 10; ++i)
						cout << indent2 << "size: " << readUint32(12 + i * 4)
										<< '\n';
				}
				break;
			}

		case name2Id("stco"): {  // Sample chunk offset atoms.
				uint32_t entries = readUint32(4);
				cout << indent1 << "Entries: " << entries << '\n';
				for (unsigned int i = 0; i < entries && i < 10; ++i) {
					cout << indent2 << "chunk: " << readUint32(8 + i * 4)
									<< '\n';
				}
				break;
			}

		case name2Id("co64"): {  // Sample chunk offset atoms.
				uint32_t entries = readUint32(4);
				cout << indent1 << "Entries: " << entries << '\n';
				for (unsigned int i = 0; i < entries && i < 10; ++i) {
					cout << indent2 << "chunk: " << readUint64(8 + i * 8)
									<< '\n';
				}
				break;
			}

		default:
			break;
	}

	for (const auto& child : children_)
		child->print(indentation + kIndentStep);
}


const char* Atom::fullName() const {
	const char* full_name = atomFullName(id_);
	return (full_name) ? full_name : "";
}

string Atom::completeName() const {
	return atomCompleteName2(name_, atomFullName(id_));
}

string Atom::completeName(uint32_t id) {
	char name[5] = {};
	id2Name(name, id);
	return atomCompleteName2(name, atomFullName(id));
}

string Atom::completeName(const char* name) {
	return atomCompleteName2(name, atomFullName(name2Id(name)));
}


bool Atom::isParent() const {
	AtomDefinition def = definition(id_);
	return def.container_state == PARENT_ATOM;
		//|| def.container_state == DUAL_STATE_ATOM;
}

bool Atom::isDual() const {
	AtomDefinition def = definition(id_);
	return def.container_state == DUAL_STATE_ATOM;
}

bool Atom::isVersioned() const {
	AtomDefinition def = definition(id_);
	return def.box_type == VERSIONED_ATOM;
}


// Manipulate direct children.
Atom* Atom::findChild(uint32_t id) const {
	for (const auto& child : children_)
		if (child->id_ == id) return child;
	return nullptr;
}

void Atom::addChild(Atom*&& newchild) {
	if (!newchild) return;
	length_ += newchild->length_;
	children_.push_back(exchange(newchild, nullptr));
}

Atom* Atom::removeChild(uint32_t id) {
	Atom* child = findChild(id);
	return (!child) ? child : removeAtom(child);
}

bool Atom::replaceChild(uint32_t id, Atom*&& replacement) {
	Atom* child = findChild(id);
	if (!child) {
		delete exchange(replacement, nullptr);
		return false;
	}
	return replaceAtom(child, forward<Atom*>(replacement));
}


// Manipulate any descendant (any child under this).
vector<Atom*> Atom::findAllAtoms(uint32_t id) const {
	vector<Atom*> atoms;
	for (const auto& child : children_) {
		if (child->id_ == id) atoms.push_back(child);
		vector<Atom*> as = child->findAllAtoms(id);
		atoms.insert(atoms.end(), as.begin(), as.end());
	}
	return atoms;
}

Atom* Atom::find1stAtom(uint32_t id) const {
	for (const auto& child : children_)
		if (child->id_ == id) return child;
	for (const auto& child : children_) {
		Atom *a = child->find1stAtom(id);
		if (a) return a;
	}
	return nullptr;
}

Atom* Atom::removeAtom(const Atom* a) {
	if (a) {
		auto it = std::find(children_.begin(), children_.end(), a);
		if (it != children_.end()) {
			Atom* orig = exchange(*it, nullptr);
			children_.erase(it);
			length_ -= orig->length_;
			return orig;
		}
	} else {
		for (auto it = children_.begin(); it != children_.end();) {
			if (*it) {
				++it;
				continue;
			}
			it = children_.erase(it);
		}
	}
	for (auto& child : children_) {
		Atom* orig = child->removeAtom(a);
		if (orig) return orig;
	}
	return nullptr;
}

Atom* Atom::exchangeAtomMoveIfFound(const Atom* original, Atom*&& replacement) {
	if (!original || !replacement) return nullptr;

	for (auto& child : children_) {
		if (child == original) {
			Atom* repl = exchange(replacement, nullptr);
			repl->file_atom_pos_ = child->file_atom_pos_;  // Why?
			length_ = length_ - child->length_ + repl->length_;
			return exchange(child, repl);
		}
	}
	for (auto& child : children_) {
		Atom* orig = child->exchangeAtomMoveIfFound(
								original, forward<Atom*>(replacement));
		if (orig) return orig;
	}
	// Don't delete replacement if original was not found.
	return nullptr;
}

bool Atom::replaceAtom(const Atom* original, Atom*&& replacement) {
	if (!replacement) return removeAtom(original);
	Atom* orig = exchangeAtomMoveIfFound(original, forward<Atom*>(replacement));
	if (!orig) {
		delete exchange(replacement, nullptr);
		return false;
	}
	delete orig;
	return true;
}


void Atom::pruneAtoms(uint32_t id) {
	if (children_.empty()) return;

	// For AtomWrite the contentSize() may include the children.
	length_ = kMinHeaderSize;

	for (auto it = children_.begin(); it != children_.end();) {
		if ((*it)->id_ != id) {
			++it;
			continue;
		}
		delete exchange(*it, nullptr);
		it = children_.erase(it);
	}
	for (auto& child : children_) {
		child->pruneAtoms(id);
		length_ += child->length_;
	}
}

void Atom::updateLength() {
	length_ = headerSize() + contentSize();

	for (const auto& child : children_) {
		child->updateLength();
		length_ += child->length_;
	}
}


// Atom content.
const uchar* Atom::content(int64_t offset) const {
	if (offset < 0) {
		throw string("Offset before beginning of content");
		return nullptr;
	}
	if (uint64_t(offset) > content_.size()) {
		throw string("Requested content offset out of range");
		return nullptr;
	}

    return &content_[offset];
}

const uchar* Atom::content(int64_t offset, size_t size) const {
	if (offset < 0) {
		throw string("Offset before beginning of content");
		return nullptr;
	}
	if (size > content_.size() ||
		uint64_t(offset) > content_.size() - size) {
		throw string("Requested content out of range");
		return nullptr;
	}

    return &content_[offset];
}

uchar* Atom::contentWrite(int64_t offset, size_t size) {
	return const_cast<uchar*>(Atom::content(offset, size));
}

size_t Atom::contentSize(int64_t offset) const {
	return (offset < 0 || uint64_t(offset) >= contentSize()) ? 0U :
			(contentSize() - offset);
}

void Atom::contentResize(size_t newsize) {
	size_t oldsize = content_.size();
	content_.resize(newsize);
	if (content_.size() != oldsize)
		length_ = length_ - oldsize + content_.size();
}


// Read content.
uint8_t Atom::readUint8(int64_t offset) const {
	const uchar* p = content(offset, sizeof(uint8_t));
	return (p) ? uint8_t(*p) : 0U;
}

uint16_t Atom::readUint16(int64_t offset) const {
	const uchar* p = content(offset, sizeof(uint16_t));
	return (p) ? readBE<uint16_t>(p) : 0U;
}

uint32_t Atom::readUint24(int64_t offset) const {
	const size_t kValueReadSize = sizeof(uint8_t) * 3;
	const uchar* p = content(offset, kValueReadSize);
	return (p) ? readBE<uint32_t,kValueReadSize>(p) : 0U;
}

uint32_t Atom::readUint32(int64_t offset) const {
	const uchar* p = content(offset, sizeof(uint32_t));
	return (p) ? readBE<uint32_t>(p) : 0U;
}

uint64_t Atom::readUint64(int64_t offset) const {
	const uchar* p = content(offset, sizeof(uint64_t));
	return (p) ? readBE<uint64_t>(p) : 0UL;
}

void Atom::readChar(char* str, int64_t offset, size_t length) const {
	assert(str);
	const uchar* p = content(offset, length);
	if (p) {
		for (unsigned int i = 0; i < length; ++i)
			*str++ = *p++;
	}
	*str = '\0';
}


// Write content.
void Atom::writeUint8(int64_t offset, uint8_t value) {
	uchar* p = contentWrite(offset, sizeof(value));
	if (p) writeBE(p, value);
}

void Atom::writeUint16(int64_t offset, uint16_t value) {
	uchar* p = contentWrite(offset, sizeof(value));
	if (p) writeBE(p, value);
}

void Atom::writeUint24(int64_t offset, uint32_t value) {
	const size_t kValueWriteSize = sizeof(uint8_t) * 3;
	uchar* p = contentWrite(offset, kValueWriteSize);
	if (p) writeBE<uint32_t,kValueWriteSize>(p, value);
}

void Atom::writeUint32(int64_t offset, uint32_t value) {
	uchar* p = contentWrite(offset, sizeof(value));
	if (p) writeBE(p, value);
}

void Atom::writeUint64(int64_t offset, uint64_t value) {
	uchar* p = contentWrite(offset, sizeof(value));
	if (p) writeBE(p, value);
}

void Atom::writeChar(int64_t offset, const char* source, size_t length) {
	uchar* p = contentWrite(offset, length);
	if (p) memcpy(p, source, length);
}


//
// AtomWrite Class.
//
AtomWrite::AtomWrite(const string& filename) : file_read_(filename) {
	if (!file_read_)
		throw string("Could not open file: ") + filename;
}


// Atom parsing.
bool AtomWrite::parseHeader() {
	if (!Atom::parseHeader(file_read_)) return false;
	file_content_size_ =
		static_cast<size_t>(file_read_.size() - file_content_pos_);
	logg(VV, "file_content_size_ = ", file_content_size_, '\n');
	return true;
}

bool AtomWrite::nextHeader()  { return Atom::nextHeader(file_read_); }

void AtomWrite::write(FileWrite* file) {
	if (!file) return;
	logg(VV, "Write atom ", name_, ".\n");
#ifndef NDEBUG
	off_t start = file->pos();
#endif

	// Write header.
	writeHeader(file);
	// Write content.
	file_read_.seek(file_content_pos_);
	const size_t kBuffSize = 1UL * 1024 * 1024;  // 1 MiB.
	vector<char> buff(kBuffSize);
	for (size_t i = 0; i < file_content_size_; i += kBuffSize) {
		size_t towrite = min(file_content_size_ - i, kBuffSize);
		file_read_.readChar(buff.data(), towrite);
		ssize_t len = file->writeChar(buff.data(), towrite);
		//assert(len >= 0 && size_t(len) == towrite);
		if (len != kBuffSize) {
			if (len < 0)
				logg(E, "File error while writing atom: ", completeName(), ".\n");
			else if (size_t(len) != towrite)
				logg(E, "Incomplete written atom: ", completeName(), ".\n");
			break;
		}
	}
	// Write children.
	for (const auto& child : children_)
		child->write(file);

#ifndef NDEBUG
	off_t end = file->pos();
	logg(VV, "Written length: ", (end - start), " start: ", start, " end: ", end,
		     "  Atom length: ", length_, '\n');
	assert(static_cast<unsigned off_t>(end - start) == length_);
#endif
}


// Atom content.
const uchar* AtomWrite::content(int64_t offset) const {
	if (offset < 0) {
		throw string("Offset before beginning of file");
		return nullptr;
	}
	if (uint64_t(offset) > file_content_size_) {
		throw string("Requested file content offset out of range");
		return nullptr;
	}

	return file_read_.getPtr(file_content_pos_ + offset,
							 file_content_size_ - offset);
}


const uchar* AtomWrite::content(int64_t offset, size_t size) const {
	if (offset < 0) {
		throw string("Offset before beginning of file");
		return nullptr;
	}
	if (size > file_content_size_ ||
		uint64_t(offset) > file_content_size_ - size) {
		throw string("Requested file content out of range");
		return nullptr;
	}

	return file_read_.getPtr(file_content_pos_ + offset, size);
}

uchar* AtomWrite::contentWrite(int64_t, size_t) {
	throw string("Can't write to file content");
	return nullptr;
}

void AtomWrite::contentResize(size_t newsize) {
	off_t file_size = file_read_.size();
	if (file_size < 0 || file_content_pos_ > file_size ||
		newsize > static_cast<size_t>(file_size - file_content_pos_)) {
		throw string("Cannot resize file content beyond file size");
		return;
	}

	length_ = length_ - file_content_size_ + newsize;
	file_content_size_ = newsize;
}


// vim:set ts=4 sw=4 sts=4 noet:
