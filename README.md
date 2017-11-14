Untrunc
=======

Restore a damaged (truncated) mp4, m4v, mov, 3gp video. Provided you have a similar not broken video. And some luck.

You need:

* Another video file which isn't broken
* [libav](https://libav.org/)
* Basic ability to use a command line

## Installing on Ubuntu

If you have an old Ubuntu version or other operational system, you might need to manually install a recent libav version, go to the bottom of the page for instructions.

Install some pre-requisite libraries with this command:

    sudo apt-get install libavformat-dev libavcodec-dev libavutil-dev

Download the source code from GitHub at https://github.com/ponchio/untrunc

    wget https://github.com/ponchio/untrunc/archive/master.zip

Unzip the source code:

    unzip master.zip

Go into the directory where it's been unzipped:

    cd untrunc-master

Compile the source code using this command (all one line):

    g++ -o untrunc file.cpp main.cpp track.cpp atom.cpp mp4.cpp -L/usr/local/lib -lavformat -lavcodec -lavutil


## Installing on other operating system (Manual libav installation)

Download the source code from GitHub at https://github.com/ponchio/untrunc and unzip the source code.
    
Download [libav, 12.2 version](http://libav.org/releases/libav-12.2.tar.xz) from [libav download page](http://libav.org/download.html) and unzip into the untrunc source code library. Then:

    cd untrunc-master

Build the library

    cd libav-12.2
    ./configure
    make
    cd ../

Build untrunc

    g++ -o untrunc file.cpp main.cpp track.cpp atom.cpp mp4.cpp -I./libav-12.2 -L./libav-12.2/libavformat -lavformat -L./libav-12.2/libavcodec -lavcodec -L./libav-12.2/libavresample -lavresample -L./libav-12.2/libavutil -lavutil -lpthread -lz

Depending on your system and libav configure options you might need to add the flags `-lbz2 -lX11 -lvdpau` to he command line to fix errors like: `undefined reference to 'BZ2_bzDecompressInit'` or `#undefined reference to 'XOpenDisplay'`.

On OSX 10.12.6, this line has been proved successful:

    g++ -o untrunc file.cpp main.cpp track.cpp atom.cpp mp4.cpp -I./libav-12.2 -L./libav-12.2/libavformat -lavformat -L./libav-12.2/libavcodec -lavcodec -L./libav-12.2/libavresample -lavresample -L./libav-12.2/libavutil -lavutil -lpthread -lz -framework CoreFoundation -framework CoreVideo -framework VideoDecodeAcceleration -lbz2



## Arch package

Jose1711 kindly provides an arch package here: https://aur.archlinux.org/packages/untrunc-git/

## Using

You need both the broken video and an example working video (ideally from the same camera, if not the chances to fix it are slim).

Run this command in the folder where you have unzipped and compiled Untrunc but replace the /path/to/... bits with your 2 video files:

    ./untrunc /path/to/working-video.m4v /path/to/broken-video.m4v

Then it should churn away and hopefully produce a playable file called broken-video_fixed.m4v

That's it you're done!

(Thanks to Tom Sparrow for providing the guide)


### Help/Support

If you managed to recover the video, help me to find time to keep working on this software and make other people happy.
If you didn't, I need more corrupted samples to improve the program and I might solve the issue, who knows... so write me.

Donations can be made at my page, http://vcg.isti.cnr.it/~ponchio/untrunc.php, and promptly converted into beer.

Thank you.
