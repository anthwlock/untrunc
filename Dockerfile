# pull base image
FROM ubuntu:bionic AS build
LABEL stage=intermediate
ARG FF_VER=shared

# install packaged dependencies
RUN apt-get update && [ "$FF_VER" = 'shared' ] && \
	apt-get -y install --no-install-recommends libavformat-dev libavcodec-dev libavutil-dev g++ make git || \
	apt-get -y install --no-install-recommends yasm wget g++ make git ca-certificates xz-utils && \
	rm -rf /var/lib/apt/lists/*

# copy code
ADD . /untrunc-src
WORKDIR /untrunc-src

RUN apt-get update && apt-get install -y software-properties-common && \
    add-apt-repository ppa:ubuntu-toolchain-r/test && \
    apt-get update && apt-get install -y g++-9 && \
    update-alternatives --install /usr/bin/g++ g++ /usr/bin/g++-9 60


# build untrunc
#RUN /usr/bin/g++ -o untrunc *.cpp -lavformat -lavcodec -lavutil
RUN CXX=g++-9 /usr/bin/make FF_VER=$FF_VER && strip untrunc


# deliver clean image
FROM ubuntu:bionic
ARG FF_VER=shared

RUN apt-get update && [ "$FF_VER" = 'shared' ] && \
	apt-get -y install --no-install-recommends libavformat57 libavcodec57 libavutil55 && \
	rm -rf /var/lib/apt/lists/* || true
COPY --from=build /untrunc-src/untrunc /bin/untrunc

# non-root user
RUN useradd untrunc
USER untrunc

# execution
ENTRYPOINT ["/bin/untrunc"]
