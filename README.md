Untrunc
=======

Restore a damaged (truncated) mp4, m4v, mov, 3gp video. Provided you have a similar not broken video. And some luck.

You need:

* Another video file which isn't broken
* Basic ability to use a command line


## Building

If you have an old Ubuntu version or other operational system, you might need to manually install a recent Libav version, go to the bottom of the page for instructions.

Install some pre-requisite libraries with this command:

    sudo apt-get install libavformat-dev libavcodec-dev libavutil-dev

Get the source code.

Compile the source code using this command (all one line):

    g++ -o untrunc -O3 *.cpp -lavformat -lavcodec -lavutil


## Using

You need both the broken video and an example working video (ideally from the same camera, if not the chances to fix it are slim).

Run this command in the folder where you have unzipped and compiled Untrunc but replace the `/path/to/...` bits with your 2 video files:

    ./untrunc /path/to/working-video.m4v /path/to/broken-video.m4v

Then it should churn away and hopefully produce a playable file called `broken-video_fixed.m4v`.

That's it you're done!

(Thanks to Tom Sparrow for providing the guide)

## Reporting issues

Use the `-v` parameter for a more detailed output. Both the healthy and corrupt file might be needed to help you.

### Help/Support

If you managed to recover the video, help me to find time to keep working on this software and make other people happy.
If you didn't, I need more corrupted samples to improve the program and I might solve the issue, who knows... so write me.

Donations can be made at my page, http://vcg.isti.cnr.it/~ponchio/untrunc.php, and promptly converted into beer.

Thank you.
