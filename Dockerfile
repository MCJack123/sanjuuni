FROM alpine:3.20.2 AS base

FROM base AS build

COPY configure Makefile.in /tmp/sanjuuni/
COPY src /tmp/sanjuuni/src

RUN set -eux; \
    apk add --no-cache --update \
    g++ \
    zlib-dev \
    poco-dev \
    make \
    ffmpeg-dev; \
    cd /tmp/sanjuuni; \
    ./configure; \
    make

FROM base

RUN set -eux; \
    apk add --no-cache --update \
    libgcc \
    zlib \
    poco \
    ffmpeg-libavformat \
    ffmpeg-libavfilter \
    ffmpeg-libavdevice \
    ffmpeg-libswscale

COPY --from=build /tmp/sanjuuni/sanjuuni /usr/local/bin

WORKDIR /srv/sanjuuni

ENTRYPOINT ["sanjuuni"]
