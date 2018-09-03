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


// Atom.
class Atom {
public:
	Atom() = default;
	explicit Atom(uint32_t id);
	explicit Atom(const char* name);
	virtual ~Atom();

	// Is a non-root atom valid?
	explicit operator bool() const { return (id_ && length_valid_); }

	uint32_t    id()       const { return id_; }
	const char* name()     const { return name_; }
	const char* fullName() const;
	std::string completeName() const;
	static std::string completeName(uint32_t id);
	static std::string completeName(const char* name);

	uint64_t length()      const { return length_; }
	size_t   headerSize()  const { return kMinHeaderSize; }
	bool     isParent()    const;
	bool     isDual()      const;
	bool     isVersioned() const;

	// Atom parsing.
	bool parseHeader(const FileRead& file);  // Read just the name and length.
	bool nextHeader(const FileRead& file);   // Seek to next atom header.
	bool parse(const FileRead& file);
	virtual void write(FileWrite* file);

	void print(int indentation = 0) const;

	// Direct children.
	Atom* findChild(uint32_t    id)   const;
	Atom* findChild(const char* name) const { return findChild(name2Id(name)); }
	void  addChild(Atom*&& newchild);
	Atom* removeChild(uint32_t    id);
	Atom* removeChild(const char* name) { return removeChild(name2Id(name)); }
	bool  replaceChild(uint32_t    originalId,   Atom*&& replacement);
	bool  replaceChild(const char* originalName, Atom*&& replacement) {
		return replaceChild(name2Id(originalName),
							std::forward<Atom*>(replacement));
	}

	// Any descendant (any child under this).
	std::vector<Atom*> findAllAtoms(uint32_t id) const;
	std::vector<Atom*> findAllAtoms(const char* name) const {
		return findAllAtoms(name2Id(name));
	}
	Atom* find1stAtom(uint32_t id) const;
	Atom* find1stAtom(const char* name) const {
		return find1stAtom(name2Id(name));
	}
	Atom* removeAtom(const Atom* descendant);
	bool  replaceAtom(const Atom* original, Atom*&& replacement);
	void  pruneAtoms(uint32_t id);
	void  pruneAtoms(const char* name) { pruneAtoms(name2Id(name)); }
	void  updateLength();

	// Atom content.
	virtual const uchar* content(int64_t offset = 0L) const;
	virtual const uchar* content(int64_t offset, size_t size) const;
	off_t          contentPos()   const { return file_content_pos_; }
	virtual size_t contentSize()  const { return content_.size(); }
	size_t         contentSize(int64_t offset) const;
	virtual void   contentResize(size_t newsize);
	const uchar*   contentBegin() const { return content(); }
	const uchar*   contentEnd()   const { return content(contentSize()); }

	// Read content.
	uint8_t  readUint8(int64_t  offset) const;
	uint16_t readUint16(int64_t offset) const;
	uint32_t readUint24(int64_t offset) const;
	uint32_t readUint32(int64_t offset) const;
	uint64_t readUint64(int64_t offset) const;
	int32_t  readInt32(int64_t offset) const { return readUint32(offset); }
	int64_t  readInt64(int64_t offset) const { return readUint64(offset); }
	void     readChar(char* str, int64_t offset, size_t length) const;

	// Write content.
	void writeUint8(int64_t offset, uint8_t   value);
	void writeUint16(int64_t offset, uint16_t value);
	void writeUint24(int64_t offset, uint32_t value);
	void writeUint32(int64_t offset, uint32_t value);
	void writeUint64(int64_t offset, uint64_t value);
	void writeInt32(int64_t offset, int32_t value) {
		writeUint32(offset, value);
	}
	void writeInt64(int64_t offset, int64_t value) {
		writeUint64(offset, value);
	}
	void writeChar(int64_t offset, const char* source, size_t length);

protected:
	const size_t kMinHeaderSize = 4 + 4;
	const size_t kMaxHeaderSize = 4 + 4 + 7 * 8;

	off_t    file_atom_pos_    = -1;  // Start of atom in file including header.
	off_t    file_content_pos_ = -1;  // Begin of atom content in file.
	uint32_t id_               = 0;
	char     name_[5]          = "";
	uint64_t length_           = 0;   // Includes header size and children.
	bool     length_zero_      = false;
	bool     length_valid_     = true;
	std::vector<uchar> content_;
	std::vector<Atom*> children_;

	void writeHeader(FileWrite* file);
	Atom* exchangeAtomMoveIfFound(const Atom* original, Atom*&& replacement);
	virtual uchar* contentWrite(int64_t offset, size_t size);
};

inline bool operator==(const Atom& a, uint32_t id) { return a.id() == id; }
inline bool operator==(const Atom& a, const char* name) {
	return a.id() == name2Id(name);
}
inline bool operator!=(const Atom& a, uint32_t id) { return a.id() != id; }
inline bool operator!=(const Atom& a, const char* name) {
	return a.id() != name2Id(name);
}


// Atom for writing.
class AtomWrite : public Atom {
public:
	explicit AtomWrite(const std::string& filename);
	virtual ~AtomWrite() = default;

	bool parseHeader();  // Read just the name and length.
	bool nextHeader();   // Seek to next atom header.
	virtual void write(FileWrite* file);

	// Atom content.
	virtual const uchar* content(int64_t offset = 0L) const;
	virtual const uchar* content(int64_t offset, size_t size) const;
	virtual size_t contentSize() const { return file_content_size_; }
	using    Atom::contentSize;
	virtual void   contentResize(size_t newsize);

protected:
	size_t file_content_size_ = 0;

	// Only read from internal FileRead.
	bool parseHeader(const FileRead& file);  // Read just the name and length.
	bool nextHeader(const FileRead& file);   // Seek to next atom header.
	bool parse(const FileRead& file);

	// Disable as we can't write to FileRead.
	void writeUint8(int64_t offset, uint8_t value);
	void writeUint16(int64_t offset, uint16_t value);
	void writeUint24(int64_t offset, uint32_t value);
	void writeUint32(int64_t offset, uint32_t value);
	void writeUint64(int64_t offset, uint64_t value);
	void writeInt32(int64_t offset, int32_t value);
	void writeInt64(int64_t offset, int64_t value);
	void writeChar(int64_t offset, const char* source, size_t length);

	// Causes (run-time) error as we can't write to FileRead.
	virtual uchar* contentWrite(int64_t offset, size_t size);

private:
	mutable FileRead file_read_;
};


// vim:set ts=4 sw=4 sts=4 noet:
#endif  // ATOM_H_
