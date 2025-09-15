Untrunc
=======

Restore a damaged (truncated) mp4, m4v, mov, 3gp video. Provided you have a similar not broken video. And some luck.

You need:

* Another video file which isn't broken
* ~~Basic ability to use a command line~~ ([GUI](#GUI) exists)

## AWS Lambda Compatible Build

This is a fork specifically configured for AWS Lambda deployment. For building a static binary compatible with AWS Lambda:

```shell
# Clean any previous builds
make clean
rm -rf ffmpeg-*

# Build for Lambda
./build-lambda.sh
```

This creates a statically linked x86_64 binary that works in AWS Lambda environments. The build includes a patch for FFmpeg to fix compilation issues with newer binutils (>= 2.41).
