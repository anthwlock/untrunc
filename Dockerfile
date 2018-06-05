# pull base image
FROM ubuntu

# install packaged dependencies
RUN apt-get update
RUN apt-get -y install libavformat-dev libavcodec-dev libavutil-dev unzip g++ wget make nasm zlib1g-dev

# download and extract
RUN wget https://github.com/ponchio/untrunc/archive/master.zip
RUN unzip master.zip 
WORKDIR /untrunc-master
#RUN wget https://fossies.org/linux/misc/libav-12.3.tar.gz && tar -zxvf libav-12.3.tar.gz -C .

# build libav
#WORKDIR /untrunc-master/libav-12.3/
#RUN ./configure && make

# build untrunc
WORKDIR /untrunc-master
RUN /usr/bin/g++ -o untrunc -O3 *.cpp -lavformat -lavcodec -lavutil
#RUN /usr/bin/g++ -o untrunc -I./libav-12.3 file.cpp main.cpp track.cpp atom.cpp mp4.cpp -L./libav-12.3/libavformat -lavformat -L./libav-12.3/libavcodec -lavcodec -L./libav-12.3/libavresample -lavresample -L./libav-12.3/libavutil -lavutil -lpthread -lz

# package / push the build artifact somewhere (dockerhub, .deb, .rpm, tell me what you want)
# ... 

# execution
WORKDIR /untrunc-master
ENTRYPOINT ["./untrunc"]
