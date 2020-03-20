#ifndef ATOM_H
#define ATOM_H
extern "C" {
#include <stdint.h>
}
#include <vector>
#include <string>

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
	virtual ~Atom();

	void parseHeader(FileRead &file, bool no_check=false); //read just name and length
	void parse(FileRead &file);
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
	static off_t findNextAtomOff(FileRead& file, const Atom* start_atom, bool searching_mdat=false);

	size_t cursor_off_ = 0;  // for "stream like" read/write methods
	void seek(size_t idx) {cursor_off_ = idx;}

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

};

bool isPointingAtAtom(FileRead& file);

#endif // ATOM_H
