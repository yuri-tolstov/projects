#!/bin/sh
# tile-monitor --tile 4x4 \
#   --hvx dataplane=4-255 --hvx TLR_NETWORK=none --hvd POST=quick \

tile-monitor --hvx dataplane=4-255 \
  --hvx TLR_NETWORK=none --hvd POST=quick \
  --upload-tile-libs gxpci \
  --upload callcap /root/callcap \
  --tile dataplane \
  --quit

