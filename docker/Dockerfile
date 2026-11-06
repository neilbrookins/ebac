# EBaC (Electronic Bible and Concordance) in a container, without Home
# Assistant: EBaC in a web page with touch gestures (ttyd), and ebac-api.
# See docker/README.md for how to run it.
#
# Built from this repository (the folder above this one is the build's
# context):
#   docker build -f docker/Dockerfile -t ebac .
# or straight from GitHub, with nothing downloaded first:
#   docker build -f docker/Dockerfile -t ebac https://github.com/neilbrookins/ebac.git#main
# A prebuilt image of each change to main is ghcr.io/neilbrookins/ebac
# (made by .github/workflows/docker.yml).
#
# Kept in step with the Home Assistant app (neilbrookins/ebac-ha-app),
# which builds from the same files: the program, the web page and its
# gestures (gestures.js, make-index.sh, ebac-web), EBaC's folder in /data
# (ebac-setup), each user's session (ebac-session) and ebac-api's scripts
# (www). Only the start differs: ebac-start here, Home Assistant's
# services there. The same Alpine release as the app's base image, so the
# same ttyd.

ARG ALPINE=alpine:3.24

# ---- Stage 1: compile EBaC ----
FROM ${ALPINE} AS build

# EBAC_COMMIT, as EBaC's Information screen shows it ("abc1234,
# 2026-11-05"): given by the workflow; otherwise from git, if the build
# kept the repository's .git (BUILDKIT_CONTEXT_KEEP_GIT_DIR=1), or blank.
ARG EBAC_COMMIT=
RUN apk add --no-cache build-base ncurses-dev git
WORKDIR /src
COPY . .
RUN if [ -n "$EBAC_COMMIT" ]; then set -- EBAC_COMMIT="$EBAC_COMMIT"; else set --; fi \
    && git config --global --add safe.directory /src \
    && make "$@" EBAC_PACKAGE="Docker" \
    && mkdir -p /out \
    && cp bible ebac-api ebac-admin index add-translation.sh add-gutenberg.sh trans groups groups.* spoken said /out/ \
    && cat index.c both.h utf8.c utf8.h | sha256sum | cut -c1-64 > /out/index.version

# ---- Stage 2: the King James text, with its index ----
# A stage, and so a layer of the image, of its own, made only from the
# text and the index program (which is the same, byte for byte, from the
# same source): when neither has changed, the build reuses it as it was
# (the workflow keeps the build's cache), so every version of the image
# shares the one copy (about 14 MB), stored once, and an update doesn't
# download it again (November 5, 2026).
FROM ${ALPINE} AS kjv
COPY --from=build /src/index /usr/local/bin/ebac-index
COPY AV /kjv/AV
RUN cd /kjv/AV && ebac-index -y > /dev/null

# ---- Stage 3: EBaC ----
# Its layers, from those that change least to those that change most, so
# an update downloads only what changed (and what comes after it): the
# packages, the King James text, then EBaC's program, scripts and page.
FROM ${ALPINE}

# PACKAGES_REFRESH (the workflow gives the week, "2026-45"): the packages
# below are installed afresh at most once a week, for Alpine's fixes;
# within the week, each version of the image shares the same layer.
ARG PACKAGES_REFRESH=
# ttyd: the web terminal. ncurses-terminfo-base: its terminal
# (xterm-256color). curl and ca-certificates: the translation importers'
# downloads. busybox-extras: BusyBox's web server, httpd, which runs
# ebac-api. tini: a proper first process, so the container stops at once.
RUN apk add --no-cache ttyd ncurses-libs ncurses-terminfo-base curl ca-certificates \
        busybox-extras tini

COPY --from=kjv /kjv/AV /usr/share/ebac/AV
COPY --from=build /out /usr/share/ebac
COPY docker/ebac-setup docker/ebac-session docker/ebac-web docker/ebac-start /usr/bin/
COPY docker/www /usr/share/ebac-www

# The web page: ttyd's own, with touch gestures added, made from the ttyd
# installed above so the two always match.
COPY docker/gestures.js docker/make-index.sh /tmp/
RUN sh /tmp/make-index.sh /tmp/gestures.js /usr/share/ebac/index.html \
    && rm /tmp/gestures.js /tmp/make-index.sh \
    && chmod +x /usr/bin/ebac-* /usr/share/ebac-www/cgi-bin/*

VOLUME /data
EXPOSE 7681 8080
ENTRYPOINT ["/sbin/tini", "--", "/usr/bin/ebac-start"]

LABEL \
    org.opencontainers.image.title="EBaC" \
    org.opencontainers.image.description="Electronic Bible and Concordance, in a web page with touch gestures" \
    org.opencontainers.image.source="https://github.com/neilbrookins/ebac" \
    org.opencontainers.image.licenses="GPL-2.0"
