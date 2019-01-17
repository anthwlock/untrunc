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
	int64_t start_;       //including 8 header bytes
	int64_t length_;      //including 8 header bytes
	char name_[5];
	char head_[4];
	char version_[4];
	size_t content_size_;
	std::vector<uchar> content_;
	std::vector<Atom *> children_;

	Atom(): start_(0), length_(-1) {
		name_[0] = name_[1] = name_[2] = name_[3] = name_[4] = 0;
		length_ = 0;
		start_ = 0;
	}
	virtual ~Atom();

	void parseHeader(FileRead &file); //read just name and length
	void parse(FileRead &file);
	virtual void write(FileWrite &file);
	void print(int offset);

	std::vector<Atom *> atomsByName(std::string name);
	Atom *atomByName(std::string name);
	void replace(Atom *original, Atom *replacement);

	void prune(std::string name);
	virtual void updateLength();

	virtual int64_t contentSize() { return content_.size(); }

	static bool isParent(char *id);
	static bool isDual(char *id);
	static bool isVersioned(char *id);

	virtual uint readInt(int64_t offset);
	int64_t readInt64(int64_t offset);
	void writeInt(int value, uint64_t offset);
	void readChar(char *str, int64_t offset, int64_t length);

	void writeInt64(int64_t value, uint64_t offset);
};

class BufferedAtom: public Atom {
public:
	FileRead& file_read_;
	int64_t file_begin_;
	int64_t file_end_;

	explicit BufferedAtom(FileRead&);
	~BufferedAtom();
	const uchar *getFragment(int64_t offset, int64_t size);
	int64_t contentSize() { return file_end_ - file_begin_; }
	void updateLength();

	uint readInt(int64_t offset);
	void write(FileWrite &file);

};

#endif // ATOM_H
