#include "mp4.h"
#include "atom.h"

#include <iostream>
#include <string>
using namespace std;

void usage() {
    cerr << "Usage: untrunc [options] <ok.mp4> [<corrupt.mp4>]\n\n";
}

int main(int argc, char *argv[]) {

    bool info = false;
    bool analyze = false;
    int i = 1;
    for(; i < argc; i++) {
        string arg(argv[i]);
        if(arg[0] == '-') {
            if(arg[1] == 'i') info = true;
            if(arg[1] == 'a') analyze = true;
        } else
            break;
    }
    if(argc == i) {
        usage();
        return -1;
    }

    string ok = argv[i];
    string corrupt;
    i++;
    if(i < argc)
        corrupt = argv[i];

    cout << "Reading: " << ok << endl;
    Mp4 mp4;

    try {
        mp4.open(ok);
        if(info) {
            mp4.printMediaInfo();
            mp4.printAtoms();
        }
        if(analyze) {
            mp4.analyze();
        }
        if(corrupt.size()) {
            mp4.repair(corrupt);
            mp4.saveVideo(corrupt + "_fixed.mp4");
        }
    } catch(string e) {
        cerr << e << endl;
        return -1;
    }
    return 0;
}
