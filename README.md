Untrunc
=======

Restore a damaged (truncated) mp4, m4v, mov, 3gp video. Provided you have a similar not broken video. And some luck.

You need:

* Another video file which isn't broken
* [Libav](https://libav.org/)
* Basic ability to use a command line


## Installing on Ubuntu

If you have an old Ubuntu version or other operational system, you might need to manually install a recent Libav version, go to the bottom of the page for instructions.

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


## Installing on other operating systems (Manual Libav installation)

Because Untrunc uses Libav internal headers and internal headers are not included in application development packages, you must build Libav from source.

Download the [Untrunc](https://github.com/ponchio/untrunc) source code from GitHub:

    wget https://github.com/ponchio/untrunc/archive/master.zip

Download [Libav, 12.2 version](http://libav.org/releases/libav-12.2.tar.xz) from [Libav download page](http://libav.org/download.html):

    wget http://libav.org/releases/libav-12.2.tar.xz

Unzip the Untrunc source code:

    unzip master.zip

Unzip the Libav source code into the Untrunc source directory:

    tar -xJf libav-12.2.tar.xz -C untrunc-master

Go into the directory where it's been unzipped:

    cd untrunc-master

Build the Libav library:

    cd libav-12.2/
    ./configure
    make
    cd ..

Depending on your system you may need to install additional packages if configure complains about them.
If configure complains about `nasm/yasm not found`, you can either install nasm or yasm or tell configure not to use a stand-alone assembler (`--disable-x86asm`). If you choose the latter, you must replace the `avresample` library with the `swresample` library below.

Build the untrunc executable:

    g++ -o untrunc -I./libav-12.2 file.cpp main.cpp track.cpp atom.cpp mp4.cpp -L./libav-12.2/libavformat -lavformat -L./libav-12.2/libavcodec -lavcodec -L./libav-12.2/libavresample -lavresample -L./libav-12.2/libavutil -lavutil -lpthread -lz

Depending on your system and Libav configure options you might need to add extra flags to the command line:
- add `-lbz2`   for errors like `undefined reference to 'BZ2_bzDecompressInit'`,
- add `-llzma`  for errors like `undefined reference to 'lzma_stream_decoder'`,
- add `-lX11`   for errors like `undefined reference to 'XOpenDisplay'`,
- add `-lvdpau` for errors like `undefined reference to 'VDPAU...'`,
- add `-ldl`    for errors like `undefined reference to 'dlopen'`.

On macOS add the following (tested on OSX 10.12.6):
- add `-framework CoreFoundation -framework CoreVideo -framework VideoDecodeAcceleration`.

If you configured Libav without using a stand-alone assembler (`--disable-x86asm`), you must substitute `avresample` with `swresample` in the command above.


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
