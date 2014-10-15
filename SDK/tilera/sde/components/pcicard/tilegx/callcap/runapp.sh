#!/bin/sh
# tile-monitor --tile 4x4 \
#  --hvx dataplane=4-255 --hvx TLR_NETWORK=none --hvd POST=quick \
#  --mkdir /root/bin --cd /root/bin \
#  --upload callcap /root/bin/callcap \
#  -- callcap

tile-monitor --hvx dataplane=4-255 \
  --hvx TLR_NETWORK=none --hvd POST=quick \
  --cd /root \
  --upload-tile-libs lua \
  --upload-tile-libs readline \
  --upload-tile-libs ncurses \
  --upload-tile-libs m \
  --upload-tile-libs tinfo \
  --upload-tile-libs gxpci \
  --upload-tile /usr/bin/lua \
  --upload callcap /root/callcap \
  --tile dataplane \
  -- callcap

