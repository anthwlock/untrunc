//==================================================================//
/*                                                                  *
	Untrunc - atom.h

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

#ifndef ATOM_H_
#define ATOM_H_

#include <cstdint>
#include <string>
#include <vector>

#include "common.h"
#include "file.h"


class Atom {
public:
	// Direct-access Attributes.
	int64_t start_;   // Includes 8 header bytes.
	int64_t length_;  // Includes 8 header bytes.
	char name_[5];
	char head_[4];
	char version_[4];
	std::vector<uchar> content_;
	std::vector<Atom*> children_;

	Atom() : start_(0), length_(-1) {
		name_[0] = name_[1] = name_[2] = name_[3] = name_[4] = 0;
		length_ = 0;
		start_ = 0;
	}
	virtual ~Atom();

	void parseHeader(FileRead& file);  // Read just name and length.
	void parse(FileRead& file);
	virtual void write(FileWrite& file);
	void print(int indentation);

	std::vector<Atom*> atomsByName(const std::string& name) const;
	Atom* atomByName(const std::string& name) const;
	void replace(const Atom* original, Atom* replacement);

	void prune(const std::string& name);
	virtual void updateLength();

	virtual int64_t contentSize() { return content_.size(); }

	static bool isParent(const char* id);
	static bool isDual(const char* id);
	static bool isVersioned(const char* id);

	virtual int readInt(int64_t offset);
	void writeInt(int value, int64_t offset);
	void readChar(char* str, int64_t offset, int64_t length);
};


class WriteAtom : public Atom {
public:
	// Direct-access Attributes.
	int64_t file_begin_;
	int64_t file_end_;

	explicit WriteAtom(FileRead& file);
	~WriteAtom();

	const uchar* getFragment(int64_t offset, int64_t size);
	virtual void updateLength();

	virtual int64_t contentSize() const { return file_end_ - file_begin_; }

	virtual int readInt(int64_t offset);
	void write(FileWrite& file);

private:
	FileRead& file_read_;
};


// vim:set ts=4 sw=4 sts=4 noet:
#endif  // ATOM_H_
