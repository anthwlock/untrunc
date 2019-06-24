/*
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

							*/

#include <iostream>
#include <string>

#include "libavutil/ffversion.h"

#include "mp4.h"
#include "atom.h"
#include "common.h"

using namespace std;

void usage() {
	cerr << "Usage: untrunc [options] <ok.mp4> [corrupt.mp4]\n"
	     << "\noptions:\n"
	     << "-V  - version\n"
	     << "-s  - skip/ignore unknown sequences\n"
	     << "-a  - analyze\n"
	     << "-i  - info\n"
	     << "-f  - find all atoms and check their lenghts\n"
	     << "-sv - stretches video to match audio duration (beta)\n"
	     << "-w  - show hidden warnings\n"
	     << "-v  - verbose\n"
		 << "-vv - more verbose\n"
	     << "-q  - only errors\n"
	     << "-n  - no interactive\n"; // in Mp4::analyze()
	exit(-1);
}

void printVersion() {
	cout << g_version_str << '\n';
	exit(0);
}

int main(int argc, char *argv[]) {

	bool info = false;
	bool analyze = false;
	bool find_atoms = false;
	int i = 1;
	for (; i < argc; i++) {
		string arg = argv[i];
		if (arg == "--version") printVersion();
		if (arg[0] == '-') {
			if(arg[1] == 'i') info = true;
			else if(arg[1] == 's' && arg[2] == 'v') g_stretch_video = true;
			else if(arg[1] == 's') g_ignore_unknown = true;
			else if(arg[1] == 'a') analyze = true;
			else if(arg[1] == 'V') printVersion();
			else if(arg[1] == 'v' && arg[2] == 'v') g_log_mode = LogMode::VV;
			else if(arg[1] == 'v') g_log_mode = LogMode::V;
			else if(arg[1] == 'w') g_log_mode = LogMode::W2;
			else if(arg[1] == 'q') g_log_mode = LogMode::E;
			else if(arg[1] == 'n') g_interactive = false;
			else if(arg[1] == 'f') find_atoms = true;
			else usage();
		}
		else if (argc > i+2) usage();  // too many arguments
		else break;
	}
	if(argc == i) usage();  // no filename given

	string ok = argv[i];
	string corrupt;
	i++;
	if(i < argc)
		corrupt = argv[i];

	bool skip_info = find_atoms;
	if (!skip_info) {
		if (g_ignore_unknown && is_new_ffmpeg_api) {
			cout << "WARNING: Because of internal decoder changes, using ffmpeg '" FFMPEG_VERSION "' with '-s' can be slow!\n"
			     << "         You are advised to compile untrunc against ffmpeg 3.3!\n"
			     << "         See the README.md on how to do that. Press [ENTER] to continue ... ";
			if (g_interactive) getchar();
		}
		logg(I, g_version_str, '\n');
	}

	try {
		if (find_atoms) { Atom::findAtomNames(ok); return 0;}

		logg(I, "reading ", ok, '\n');
		Mp4 mp4;
		mp4.parseOk(ok);
		if(info) {
			mp4.printMediaInfo();
			mp4.printAtoms();
		}
		if(analyze) {
			mp4.analyze(ok);
		}
		if(corrupt.size()) {
			mp4.repair(corrupt, corrupt + "_fixed.mp4");
		}
	}
	catch(const char* e) {return cerr << e << '\n', 1;}
	catch(string e) {return cerr << e << '\n', 1;}

	chkHiddenWarnings();
	return 0;
}
