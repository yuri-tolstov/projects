#!/bin/sh
tile-monitor --hvx TLR_NETWORK=gbe1,gbe2,gbe3,gbe4 --hvd POST=quick \
  --upload-tile-libs gxpci \
  --upload callcap /root/callcap \
  --quit
 
