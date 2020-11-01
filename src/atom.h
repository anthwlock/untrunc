#ifndef ATOM_H
#define ATOM_H
extern "C" {
#include <stdint.h>
}
#include <vector>
#include <string>
#include <cassert>

#include "common.h"
#include "file.h"

class Atom {
public:
	int64_t start_ = -8;  // including 8 header bytes
	int64_t length_ = -1;  // including 8 header bytes
	int64_t header_length_ = 8;  // 8 or 16
	std::string name_;
	std::vector<uchar> content_;
	std::vector<Atom *> children_;

	Atom();
	Atom(FileRead& f) {parse(f);}
	virtual ~Atom();

	void parseHeader(FileRead &file, bool no_check=false); //read just name and length
	void parse(FileRead& file);
	virtual void write(FileWrite &file);
	void print(int offset);

	std::vector<Atom *> atomsByName(const std::string& name, bool no_recursive=false);
	Atom *atomByName(const std::string& name, bool no_recursive=false);
	Atom *atomByNameSafe(const std::string& name);
	void replace(Atom *original, Atom *replacement);

	void prune(const std::string& name);
	void prune(Atom* child);
	void updateLength();

	virtual int64_t contentSize() const { return content_.size(); }
	int64_t contentStart() const { return start_ + header_length_; }

	static bool isParent(const std::string& id);
	static bool isDual(const std::string& id);  // not used
	static bool isVersioned(const std::string& id);  // neither

	virtual uint readInt(off_t offset);
	uint readInt();
	int64_t readInt64(off_t offset);
	std::string getString(off_t offset, int64_t length);

	void writeInt(int value);
	void writeInt(int value, off_t offset);
	void writeInt64(int64_t value);
	void writeInt64(int64_t value, off_t offset);

	static void findAtomNames(const std::string& filename);
	static off_t findNextAtomOff(FileRead& file, const Atom* start_atom, bool searching_rootlvl=false);

	size_t cursor_off_ = 0;  // for "stream like" read/write methods
	void seek(size_t idx) {cursor_off_ = idx;}

	static const off_t kEndAtomStart = -55;
	static Atom mkEndAtom() {
		return Atom(kEndAtomStart);
	}

private:
	Atom(off_t start) : start_(start) {}

};

class BufferedAtom: public Atom {
public:
	FileRead& file_read_;
	int64_t file_end_;

	explicit BufferedAtom(FileRead&);

	int64_t contentSize() const { return file_end_ - contentStart(); }
	const uchar *getFragment(off_t offset, int size);
	uint readInt(off_t offset);
	int newHeaderSize() { return needs64bitVersion() ? 16 : 8; }
	bool needs64bitVersion();

	std::vector<std::pair<off_t, uint64_t>> sequences_to_exclude_;  // from resulting mdat
	int64_t total_excluded_yet_ = 0;

	void write(FileWrite &file, bool force_64=false);

	void updateFileEnd(int64_t file_end);
};


class HiddenAtomIt {
public:
	FileRead& f_;
	Atom atom_;
	HiddenAtomIt(FileRead& f) : f_(f) { operator++(); }
	HiddenAtomIt mkEndIt() { return HiddenAtomIt(f_, true); }

	void operator++() {
		off_t off = Atom::findNextAtomOff(f_, &atom_);
		if (off >= f_.length()) {
			atom_.start_ = Atom::kEndAtomStart;
			return;
		}
		f_.seek(off);
		atom_.parseHeader(f_, true);
	}
	Atom& operator*() {return atom_;}
	bool operator!=(HiddenAtomIt rhs) {return atom_.start_ != rhs.atom_.start_;}

private:
	HiddenAtomIt(FileRead& f, bool is_end_it_) : f_(f), atom_(Atom::mkEndAtom()) { assert(is_end_it_); }
};

/* Iteratable, which also lists 'hidden' atoms */
class AllAtomsIn {
public:
	HiddenAtomIt it_, end_it_;
	AllAtomsIn(FileRead& f) : it_(f), end_it_(it_.mkEndIt()) {}
	HiddenAtomIt begin() {return it_;}
	HiddenAtomIt end() {return end_it_;}
};


bool isPointingAtAtom(FileRead& file);
bool isValidAtomName(const uchar* buf);

#endif // ATOM_H
