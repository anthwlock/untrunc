# pull base image
FROM ubuntu

# install packaged dependencies
RUN apt-get update
RUN apt-get -y install libavformat-dev libavcodec-dev libavutil-dev g++ make

# in case we alredy have the src (travis)
ADD . /untrunc-src

# otherwise download and extract master
RUN [ -f /untrunc-src/mp4.cpp ] || apt-get -y install wget unzip
RUN [ -f /untrunc-src/mp4.cpp ] || wget https://github.com/ponchio/untrunc/archive/master.zip
RUN [ -f /untrunc-src/mp4.cpp ] || unzip master.zip 
RUN [ -f /untrunc-src/mp4.cpp ] || mv /untrunc-src /abc
RUN [ -f /untrunc-src/mp4.cpp ] || ln -s untrunc-master /untrunc-src

WORKDIR /untrunc-src

# build untrunc
#RUN /usr/bin/g++ -o untrunc *.cpp -lavformat -lavcodec -lavutil
RUN /usr/bin/make

# package / push the build artifact somewhere (dockerhub, .deb, .rpm, tell me what you want)
# ... 

# execution
ENTRYPOINT ["./untrunc"]
