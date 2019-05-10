/*
	Untrunc - mp4.h

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

							*/

#ifndef MP4_H
#define MP4_H

#include <vector>
#include <string>
#include <stdio.h>

#include "track.h"
class Atom;
class BufferedAtom;
class FileRead;
class AVFormatContext;

class Mp4 {
public:
	int timescale_;
	int duration_ = 0;
	Atom *root_atom_;

	Mp4();
	~Mp4();

	void parseOk(std::string& filename); // parse the healthy one

	void printMediaInfo();
	void printAtoms();
	void makeStreamable(std::string& filename, std::string& output);

	void analyze(const std::string& filename);
	void repair(std::string& filename, const std::string& filname_fixed);

protected:
	BufferedAtom* findMdat(FileRead& file_read);
	std::vector<Track> tracks_;
	AVFormatContext *context_;

	void saveVideo(const std::string& filename);
	void parseTracksOk();
	void chkStrechFactor();
	void setDuration();
	bool broken_is_64_ = false;
	int unknown_length_ = 0;
	std::vector<int> unknown_lengths_;
};


#endif // MP4_H
