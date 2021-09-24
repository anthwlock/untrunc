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
	     << "-dr - dump repaired tracks, implies '-dw'\n"
	     << "-k  - keep unknown sequences\n"
	     << "-sm  - search mdat, even if no mp4-structure found\n"
	     << "-dcc  - dont check if chunks are inside mdat\n"
	     << "-dyn  - use dynamic stats\n"
	     << "-range <A:B>  - raw data range\n"
	     << "-dst <dir|file>  - set destination\n"
	     << "-skip  - skip existing\n"
	     << "-noctts  - dont restore ctts\n"
	     << "-mp <bytes>  - set max partsize\n"
	     << "\n"
	     << "analyze options:\n"
	     << "-a  - analyze\n"
	     << "-i[t|a|s] - info [tracks|atoms|stats]\n"
	     << "-d  - dump samples\n"
	     << "-f  - find all atoms and check their lenghts\n"
	     << "-lsm - find all mdat,moov atoms\n"
	     << "-m <offset> - match/analyze file offset\n"
	     << "untrunc <ok.mp4> <ok.mp4> - report wrong values\n"
	     << "\n"
	     << "other options:\n"
	     << "-ms  - make streamable\n"
	     << "-sh  - shorten\n"
	     << "-u <mdat-file> <moov-file> - unite fragments\n"
	     << "\n"
	     << "logging options:\n"
	     << "-q  - quiet, only errors\n"
	     << "-w  - show hidden warnings\n"
	     << "-v  - verbose\n"
	     << "-vv - more verbose\n"
	     << "-do - don't omit potential noise\n";
	exit(-1);
}

void printVersion() {
	cout << g_version_str << '\n';
	exit(0);
}

void parseRange(const string& s) {
	auto pos = s.find(":");
	if (pos == string::npos)
		logg(ET, "use python slice notation\n");
	auto s1 = s.substr(0, pos);
	auto s2 = s.substr(pos+1);
	g_range_start = s1.size() ? stoll(s1) : 0;
	g_range_end = s2.size() ? stoll(s2) : numeric_limits<int64_t>::max();
}

int main(int argc, char *argv[]) {
	const int kExpectArg = -22;
	bool show_info = false;
	bool show_tracks = false;
	bool show_atoms = false;
	bool show_stats = false;
	bool analyze = false;
	bool find_atoms = false;
	bool dump_samples = false;
	bool analyze_offset = false;
	bool make_streamable = false;
	bool unite = false;
	bool listm = false;
	bool shorten = false;
	bool force_shorten = false;
	off_t arg_offset = -1;
	int arg_step = -1;
	int arg_range = -1;
	int arg_dst = -1;
	int arg_mp = -1;

	argv_as_utf8(argc, argv);

	int i = 1;
	for (; i < argc; i++) {
		string arg = argv[i];
		if (arg_offset == kExpectArg) {arg_offset = stoll(arg); continue;}
		if (arg_step == kExpectArg) {arg_step = stoi(arg); continue;}
		if (arg_range == kExpectArg) {parseRange(arg); arg_range = -1; continue;}
		if (arg_dst == kExpectArg) {g_dst_path = arg; arg_dst = -1; continue;}
		if (arg_mp == kExpectArg) {parseMaxPartsize(arg); arg_mp = -1; continue;}
		if (arg == "--version") printVersion();
		if (arg[0] == '-') {
			auto a = arg.substr(1);
			if      (a == "i") show_info = true;
			else if (a == "it") show_tracks = true;
			else if (a == "ia") show_atoms = true;
			else if (a == "is") show_stats = true;
			else if (a == "sm") g_search_mdat = true;
			else if (a == "sv") g_stretch_video = true;
			else if (a == "st") arg_step = kExpectArg;
			else if (a == "sh") shorten = true;
			else if (a == "fsh") force_shorten = shorten = true;
			else if (a == "lsm") listm = true;
			else if (a == "s") g_ignore_unknown = true;
			else if (a == "k") g_dont_exclude = true;
			else if (a == "a") analyze = true;
			else if (a == "V") printVersion();
			else if (a == "vv") g_log_mode = LogMode::VV;
			else if (a == "v") g_log_mode = LogMode::V;
			else if (a == "w") g_log_mode = LogMode::W2;
			else if (a == "q") g_log_mode = LogMode::E;
			else if (a == "noctts") g_no_ctts = true;
			else if (a == "n") g_interactive = false;
			else if (a == "f") find_atoms = true;
			else if (a == "do") g_dont_omit = true;
			else if (a == "dw") g_dont_write = true;
			else if (a == "dr") g_dump_repaired = true;
			else if (a == "d") {dump_samples = true; g_log_mode = LogMode::E;}
			else if (a == "m") {analyze_offset = true; arg_offset = kExpectArg; g_log_mode = LogMode::E;}
			else if (a == "ms") make_streamable = true;
			else if (a == "u") unite = true;
			else if (a == "dcc") g_ignore_out_of_bound_chunks = true;
			else if (a == "dyn") g_use_chunk_stats = true;
			else if (a == "range") arg_range = kExpectArg;
			else if (a == "dst") arg_dst = kExpectArg;
			else if (a == "skip") g_skip_existing = true;
			else if (a == "mp") arg_mp = kExpectArg;
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

	if (!g_ignore_unknown && arg_step > 0)
		logg(ET, "setting step_size without using '-s'\n");

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

	auto chkC = [&]() {
		if (!corrupt.size())
			logg(ET, "no second file specified\n");
	};

	try {
		if (find_atoms) {Atom::findAtomNames(ok); return 0;}
		if (unite) {chkC(); Mp4::unite(ok, corrupt); return 0;}
		if (shorten) {Mp4::shorten(ok, corrupt.size() ? stoi(corrupt) : 200, force_shorten); return 0;}
		if (listm) {Mp4::listm(ok); return 0;}

		Mp4 mp4;
		g_mp4 = &mp4;  // singleton is overkill, this is good enough
		if (arg_step > 0) {
			logg(I, "using step_size=", arg_step, "\n");
			Mp4::step_ = arg_step;
		}

		auto ext = getMovExtension(ok);
		if (make_streamable) { mp4.makeStreamable(ok, ok + "_streamable" + ext); return 0; }
		if (mp4.alreadyRepaired(ok, corrupt)) return 0;

		logg(I, "reading ", ok, '\n');
		mp4.parseOk(ok, (show_atoms || show_info));

		if (show_tracks) mp4.printTracks();
		else if (show_atoms) mp4.printAtoms();
		else if (show_stats) mp4.printDynStats();
		else if (show_info) mp4.printMediaInfo();
		else if (dump_samples) mp4.dumpSamples();
		else if (analyze) mp4.analyze();
		else if (analyze_offset) mp4.analyzeOffset(corrupt.empty() ? ok : corrupt, arg_offset);
		else if (corrupt.size()) mp4.repair(corrupt);
	}
	catch (const char* e) {return cerr << e << '\n', 1;}
	catch (string e) {return cerr << e << '\n', 1;}

	chkHiddenWarnings();
	return 0;
}
