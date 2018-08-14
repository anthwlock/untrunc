//==================================================================//
/*                                                                  *
	Untrunc - main.cpp

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

#include <iostream>
#include <string>

#include "common.h"
#include "mp4.h"


using std::cerr;
using std::cout;
using std::endl;
using std::string;


void usage() {
	cerr << "Usage: untrunc [options] <ok.mp4> [corrupt.mp4]\n"
		 << "\noptions:\n"
		 << "-a  - analyze\n"
		 << "-i  - info\n"
		 << "-v  - verbose\n"
		 << "-vv - more verbose\n"
		 << "-q  - only errors\n";
}


int main(int argc, char* argv[]) {
	bool info = false;
	bool analyze = false;
	int i = 1;
	for (; i < argc; i++) {
		string arg(argv[i]);
		if (arg[0] == '-') {
			if (arg[1] == 'i') {
				info = true;
			} else if (arg[1] == 'a') {
				analyze = true;
			} else if (arg[1] == 'v' && arg[2] == 'v') {
				g_log_mode = LogMode::VV;
			} else if (arg[1] == 'v') {
				g_log_mode = LogMode::V;
			} else if (arg[1] == 'q') {
				g_log_mode = LogMode::E;
			} else {
				usage();
				return -1;
			}
		} else if (argc > i + 2) {
			usage();
			return -1;
		} else {
			break;
		}
	}
	if (argc == i) {
		usage();
		return -1;
	}

	string ok = argv[i];
	string corrupt;
	i++;
	if (i < argc)
		corrupt = argv[i];

	logg(I, "reading ", ok, '\n');
	Mp4 mp4;

	try {
		mp4.parseOk(ok);
		if (info) {
			mp4.printMediaInfo();
			mp4.printAtoms();
		}
		if (analyze) {
			mp4.analyze();
		}
		if (corrupt.size()) {
			mp4.repair(corrupt, corrupt + "_fixed.mp4");
		}
	} catch (string e) {
		cerr << e << endl;
		return -1;
	}
	return 0;
}


// vim:set ts=4 sw=4 sts=4 noet:
