Untrunc
=======

Restore a damaged (truncated) mp4, m4v, mov, 3gp video. Provided you have a similar not broken video. And some luck.

You need:

* Another video file which isn't broken
* ~~Basic ability to use a command line~~ ([GUI](#GUI) exists)

## About this fork
This fork improves the [original](https://github.com/ponchio/untrunc) in the following:
* more than 10 times faster!
* low memory usage, fixes [#30](https://github.com/ponchio/untrunc/issues/30#issuecomment-143744821)
* easier to build + automated [windows builds](https://github.com/anthwlock/untrunc/releases/latest)
* \>2GB file support
* ability to skip over unknown bytes
* generic support for all tracks with fixed-width chunks (e.g. twos/sowt)
* advanced logging system
* can stretch/shrink video to match audio duration
* compatible with new versions of ffmpeg
* handles invalid atom lengths
* supports GoPro and Sony XAVC videos
* many bugs got fixed, actively maintained

## Building

Windows users can download the latest version [here](https://github.com/anthwlock/untrunc/releases/latest).\
In certain cases a specific version of ffmpeg is needed. Untrunc works great with ffmpeg 3.3.9.

#### With system libraries

```shell
sudo apt-get install libavformat-dev libavcodec-dev libavutil-dev
# get the source code
make
sudo cp untrunc /usr/local/bin
```

#### With local libraries

Just use following commands, make will do the rest for you.

```shell
sudo apt-get install yasm wget
make FF_VER=3.3.9
sudo cp untrunc /usr/local/bin
```

#### GUI

The GUI is optional. It is included in the automated [windows builds](https://github.com/anthwlock/untrunc/releases/latest).\
You will need [libui](https://github.com/andlabs/libui). After that, just do

```shell
make untrunc-gui
```

#### CentOS 7

```shell
sudo yum -y install epel-release && sudo yum -y install git gcc-c++ yasm
git clone --depth 5 https://github.com/anthwlock/untrunc && cd untrunc
make FF_VER=3.3.9
sudo cp untrunc /usr/local/bin
```

#### macOS with Homebrew

```
brew install ffmpeg yasm
export PKG_CONFIG_PATH="/opt/homebrew/lib/pkgconfig"
CPPFLAGS="-I/opt/homebrew/include" LDFLAGS="-L/opt/homebrew/lib" make
```


## Docker container

You can use the included Dockerfile to build and execute the package as a container.\
The optional argument 'FF_VER' will be passed to `make`.

```shell
# docker build --build-arg FF_VER=3.3.9 -t untrunc .
docker build -t untrunc .
docker image prune --filter label=stage=intermediate -f

docker run --rm -v ~/Videos/:/mnt untrunc /mnt/ok.mp4 /mnt/broken.mp4
```

## Snapcraft

If you have `snap`, you can use `sudo snap install --edge untrunc-anthwlock`.

[![untrunc-anthwlock](https://snapcraft.io//untrunc-anthwlock/badge.svg)](https://snapcraft.io/untrunc-anthwlock)

## Using

You need both the broken video and an example working video (ideally from the same camera, if not the chances to fix it are slim).

Run this command in the folder where you have unzipped and compiled Untrunc but replace the `/path/to/...` bits with your 2 video files:

```shell
./untrunc /path/to/working-video.m4v /path/to/broken-video.m4v
```

Then it should churn away and hopefully produce a playable file called `broken-video_fixed.m4v`.

That's it you're done!

(Thanks to Tom Sparrow for providing the guide)


### Help/Support

#### Reporting issues
Use the `-v` parameter for a more detailed output. Both the healthy and corrupt file might be needed to help you.

#### Donation
If this software helped you please consider donating [here](https://www.paypal.me/anthwlock)!\
Donations will encourage me to keep working on this software, leading to more media being supported and better recovered files.

You might also want to consider donating to **ponchio**, see his instructions [here](https://github.com/ponchio/untrunc#helpsupport).

Thank you.
