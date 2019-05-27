# pull base image
FROM ubuntu:bionic AS build
LABEL stage=intermediate
ARG FF_VER=shared

# install packaged dependencies
RUN apt-get update && [ "$FF_VER" = 'shared' ] && \
	apt-get -y install --no-install-recommends libavformat-dev libavcodec-dev libavutil-dev g++ make git || \
	apt-get -y install --no-install-recommends libva-dev liblzma-dev libx11-dev libbz2-dev zlib1g-dev \
		yasm pkg-config wget g++ make git ca-certificates && \
	rm -rf /var/lib/apt/lists/*

# copy code
ADD . /untrunc-src
WORKDIR /untrunc-src

# build untrunc
#RUN /usr/bin/g++ -o untrunc *.cpp -lavformat -lavcodec -lavutil
RUN /usr/bin/make FF_VER=$FF_VER && strip untrunc


# deliver clean image
FROM ubuntu:bionic
ARG FF_VER=shared

RUN apt-get update && [ "$FF_VER" = 'shared' ] && \
	apt-get -y install --no-install-recommends libavformat57 libavcodec57 libavutil55 || \
	apt-get -y install --no-install-recommends libx11-6 libva-drm2 libva2 libva-x11-2 && \
	rm -rf /var/lib/apt/lists/*
COPY --from=build /untrunc-src/untrunc /bin/untrunc

# non-root user
RUN useradd untrunc
USER untrunc

# execution
ENTRYPOINT ["/bin/untrunc"]
