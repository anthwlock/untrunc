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
	     << "\ngeneral options:\n"
	     << "-V  - version\n"
	     << "-n  - no interactive\n" // in Mp4::analyze()
	     << "\n"
	     << "repair options:\n"
	     << "-s  - step through unknown sequences\n"
	     << "-st <step_size> - used with '-s'\n"
	     << "-sv - stretches video to match audio duration (beta)\n"
	     << "-dw - don't write _fixed.mp4\n"
	     << "\n"
	     << "analyze options:\n"
	     << "-a  - analyze\n"
	     << "-i  - info\n"
	     << "-d  - dump samples\n"
	     << "-f  - find all atoms and check their lenghts\n"
	     << "-t  - show tracks\n"
	     << "-m <offset> - match/analyze file offset\n"
	     << "untrunc <ok.mp4> <ok.mp4> - report wrong values\n"
	     << "\n"
	     << "logging options:\n"
	     << "-q  - quiet, only errors\n"
	     << "-w  - show hidden warnings\n"
	     << "-v  - verbose\n"
	     << "-vv - more verbose\n";
	exit(-1);
}

void printVersion() {
	cout << g_version_str << '\n';
	exit(0);
}

int main(int argc, char *argv[]) {

	const int kExpectArg = -22;

	bool show_info = false;
	bool show_tracks = false;
	bool analyze = false;
	bool find_atoms = false;
	bool dump_samples = false;
	bool analyze_offset = false;
	off_t arg_offset = -1;
	int arg_step = -1;
	string output_suffix;

	int i = 1;
	for (; i < argc; i++) {
		string arg = argv[i];
		if (arg_offset == kExpectArg) {arg_offset = stoll(arg); continue;}
		if (arg_step == kExpectArg) {arg_step = stoi(arg); continue;}
		if (arg == "--version") printVersion();
		if (arg[0] == '-') {
			auto a = arg.substr(1);
			if      (a == "i") show_info = true;
			else if (a == "t") show_tracks = true;
			else if (a == "sv") g_stretch_video = true;
			else if (a == "st") arg_step = kExpectArg;
			else if (a == "s") g_ignore_unknown = true;
			else if (a == "a") analyze = true;
			else if (a == "V") printVersion();
			else if (a == "vv") g_log_mode = LogMode::VV;
			else if (a == "v") g_log_mode = LogMode::V;
			else if (a == "w") g_log_mode = LogMode::W2;
			else if (a == "q") g_log_mode = LogMode::E;
			else if (a == "n") g_interactive = false;
			else if (a == "f") find_atoms = true;
			else if (a == "dw") {g_dont_write = true;}
			else if (a == "d") {dump_samples = true; g_log_mode = LogMode::E;}
			else if (a == "m") {analyze_offset = true; arg_offset = kExpectArg; g_log_mode = LogMode::E;}
			else if (arg.size() > 2) {cerr << "Error: seperate multiple options with space! See '-h'\n";  return -1;}
			else usage();
		}
		else if (argc > i+2) usage();  // too many arguments
		else break;
	}
	if (argc == i) usage();  // no filename given

	string ok = argv[i++], corrupt;
	if (i < argc) corrupt = argv[i++];

	g_show_tracks = show_tracks || show_info;

	if (!g_ignore_unknown && arg_step > 0) {
		cout << "setting step_size without using '-s'\n";
		exit(1);
	}

	bool skip_info = find_atoms;
	if (!skip_info) {
		if (g_ignore_unknown && is_new_ffmpeg_api) {
			cout << "WARNING: Because of internal decoder changes, using ffmpeg '" FFMPEG_VERSION "' with '-s' can be slow!\n"
			     << "         You are advised to compile untrunc against ffmpeg 3.3!\n"
			     << "         See the README.md on how to do that. ";
			if (false && g_interactive) {cout << "Press [ENTER] to continue ... " << flush; getchar();}
			else cout << '\n';
		}
		logg(I, g_version_str, '\n');
	}

	try {
		if (find_atoms) {Atom::findAtomNames(ok); return 0;}

		Mp4 mp4;
		g_mp4 = &mp4;  // singleton is overkill, this is good enough
		if (arg_step > 0) {
			logg(I, "using step_size=", arg_step, "\n");
			Mp4::step_ = arg_step;
		}

		if (g_ignore_unknown) output_suffix = ss("-s", Mp4::step_);


		logg(I, "reading ", ok, '\n');
		mp4.parseOk(ok);

		if (show_tracks) mp4.printMediaInfo();
		else if (analyze_offset) mp4.analyzeOffset(corrupt.empty() ? ok : corrupt, arg_offset);
		else if (dump_samples) mp4.dumpSamples();
		else if (show_info) {mp4.printMediaInfo(); cout << "\n\n"; mp4.printAtoms();}
		else if (analyze) mp4.analyze();
		else if (corrupt.size()) mp4.repair(corrupt, corrupt + "_fixed" + output_suffix + ".mp4");
	}
	catch (const char* e) {return cerr << e << '\n', 1;}
	catch (string e) {return cerr << e << '\n', 1;}

	chkHiddenWarnings();
	return 0;
}
