Untrunc
=======

Restore a damaged (truncated) mp4, m4v, mov, 3gp video. Provided you have a similar not broken video. And some luck.


You need:

* another video file which isn't broken
* a linux installation (I used Ubuntu 12.04) and basic ability to use a command line.

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

Did the code work for you? Did it save unreplaceable moments or hours of work? Help me to find the time
to improve the code and make other people happy :)

<form action="https://www.paypal.com/cgi-bin/webscr" method="post" target="_top">
<input type="hidden" name="cmd" value="_s-xclick">
<input type="hidden" name="hosted_button_id" value="WDRB464WW3AV8">
<input type="image" src="https://www.paypalobjects.com/en_US/IT/i/btn/btn_donateCC_LG.gif" border="0" name="submit" alt="PayPal - The safer, easier way to pay online!">
<img alt="" border="0" src="https://www.paypalobjects.com/en_US/i/scr/pixel.gif" width="1" height="1">
</form>





