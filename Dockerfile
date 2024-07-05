FROM alpine:3.20.1 AS base

# CVE-2024-5535 - remove when base image updated
RUN set -eux; \
    apk add --no-cache --update openssl

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
    ffmpeg

COPY --from=build /tmp/sanjuuni/sanjuuni /usr/local/bin

WORKDIR /srv/sanjuuni

ENTRYPOINT ["sanjuuni"]
