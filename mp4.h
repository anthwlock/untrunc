//==================================================================//
/*                                                                  *
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
 *                                                                  */
//==================================================================//

#ifndef MP4_H_
#define MP4_H_

#include <string>
#include <vector>

#include "track.h"


struct AVFormatContext;
class Atom;


class Mp4 {
public:
	int timescale_;
	int duration_;
	Atom* root_atom_;

	Mp4();
	~Mp4();

	void parseOk(const std::string& filename);  // Parse the healthy one.

	void printMediaInfo();
	void printAtoms();
	void makeStreamable(const std::string& filename, const std::string& output);

	void analyze();
	void writeTracksToAtoms();
	void repair(const std::string& filename, const std::string& filname_fixed);

protected:
	std::vector<Track> tracks_;
	AVFormatContext* context_;

	void saveVideo(const std::string& filename);
	void parseTracks();
};


// vim:set ts=4 sw=4 sts=4 noet:
#endif  // MP4_H_
