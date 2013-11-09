#!/bin/sh
# tile-monitor --tile 4x4 \
#  --hvx dataplane=4-255 --hvx TLR_NETWORK=none --hvd POST=quick \
#  --mkdir /root/bin --cd /root/bin \
#  --upload callcap /root/bin/callcap \
#  -- callcap

tile-monitor --hvx dataplane=4-255 \
  --hvx TLR_NETWORK=none --hvd POST=quick \
  --cd /root \
  --upload-tile-libs gxpci \
  --upload callcap /root/callcap \
  --tile dataplane \
  -- callcap

