# pull base image
FROM ubuntu:bionic

ARG FF_VER=shared

# install packaged dependencies
RUN \
  apt-get update && \
  apt-get -y install \
    libavformat-dev \
    libavcodec-dev \
    libavutil-dev \
    g++ \
    make \
    libva-dev \
    liblzma-dev \
    libx11-dev \
    libbz2-dev \
    zlib1g-dev \
    yasm \
    pkg-config \
    wget \
   && rm -rf /var/lib/apt/lists/*

# copy code
ADD . /untrunc-src
WORKDIR /untrunc-src

# build untrunc
RUN /usr/bin/make FF_VER=$FF_VER

# non-root user
RUN useradd untrunc
USER untrunc

# execution
ENTRYPOINT ["/untrunc-src/untrunc"]
