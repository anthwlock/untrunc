Untrunc
=======

Restore a damaged (truncated) mp4, m4v, mov, 3gp video. Provided you have a similar not broken video. And some luck.


You need:

* another video file which isn't broken
* a linux installation (I used Ubuntu 12.04) and basic ability to use a command line.

If you have an old Ubuntu version, you might need to manually install a recent libav version, go to the bottom of the page for instructions.

What to do:

Install some pre-requisite libraries with this command:

    sudo apt-get install libavformat-dev libavcodec-dev libavutil-dev

Download the source code from gtub at https://github.com/ponchio/untrunc

    wget https://github.com/ponchio/untrunc/archive/master.zip

Unzip the source code:

    unzip master.zip

and then go into the directory where it's been unzipped:

    cd untrunc-master

Compile the source code using this command (all one line):

    g++ -o untrunc file.cpp main.cpp track.cpp atom.cpp mp4.cpp -L/usr/local/lib -lavformat -lavcodec -lavutil

Then you can actually fix the video. You need both the broken video and an example working video (ideally from the same camera, if not the chances to fix it are slim).

Run this command in the folder where you have unzipped and compiled Untrunc but replace the /path/to/... bits with your 2 video files:

    ./untrunc /path/to/working-video.m4v /path/to/broken-video.m4v

Then it should churn away and hopefully produce a playable file called broken-video_fixed.m4v

That's it you're done!

(Thanks to Tom Sparrow for providing the guide)

##Manual libav installation

Download [libav, 0.8.7 version](http://libav.org/releases/libav-0.8.7.tar.xz) from [libav download page](http://libav.org/download.html) and
unzip it into the untrunc source code library.

    cd untrunc-master
    tar -xvzf libav-0.8.7.tar.xz

build the library

    cd libav-0.8.7
    ./configure
    ./make

and finally build untrunc
    
    g++ -o untrunc file.cpp main.cpp track.cpp atom.cpp mp4.cpp -I./libav-0.8.7 -L./libav-0.8.7/libavformat -lavformat -L./libav-0.8.7/libavcodec -lavcodec -L./libav-0.8.7/libavutil -lavutil -lpthread -lz

then proceed as above.

### Help/Support

If you managed to recover the video, help me to find time to keep working on this software and make other people happy.

Donations can be made at my page, http://vcg.isti.cnr.it/~ponchio

Thank you.






