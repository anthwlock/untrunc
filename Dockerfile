# pull base image
FROM ubuntu:22.04 AS build
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

# build untrunc
#RUN /usr/bin/g++ -o untrunc *.cpp -lavformat -lavcodec -lavutil
RUN /usr/bin/make FF_VER=$FF_VER && strip untrunc


# deliver clean image
FROM ubuntu:22.04
ARG FF_VER=shared

RUN apt-get update && [ "$FF_VER" = 'shared' ] && \
	apt-get -y install --no-install-recommends libavformat58 libavcodec58 libavutil56 && \
	rm -rf /var/lib/apt/lists/* || true
COPY --from=build /untrunc-src/untrunc /bin/untrunc

# non-root user
RUN useradd untrunc
USER untrunc

# execution
ENTRYPOINT ["/bin/untrunc"]
