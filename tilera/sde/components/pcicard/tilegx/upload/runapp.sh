#!/bin/sh
tile-monitor --hvx TLR_NETWORK=none --hvd POST=quick \
  --cd /root \
  --upload-tile-libs lua \
  --upload-tile-libs readline \
  --upload-tile-libs ncurses \
  --upload-tile-libs m \
  --upload-tile-libs tinfo \
  --upload-tile /usr/bin/lua \
  --quit

