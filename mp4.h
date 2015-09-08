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
class File;
class AVFormatContext;

class Mp4 {
public:
    int timescale;
    int duration;
    Atom *root;

    Mp4();
    ~Mp4();

    void open(std::string filename);

    void printMediaInfo();
    void printAtoms();
    void saveVideo(std::string filename);
	void makeStreamable(std::string filename, std::string output);


    void analyze();
    void writeTracksToAtoms();
    void repair(std::string filename);

protected:    
    std::vector<Track> tracks;
    AVFormatContext *context;

    void parseTracks();
};


#endif // MP4_H
