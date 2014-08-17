#!/bin/bash
DISTDIR=$(pwd)
# TFTP_ROOT=/var/lib/tftpboot
# TFTP_RELDIR=$TFTP_ROOT/centos5
HTTP_ROOT=/var/www/doc
HTTP_COSDIR=$HTTP_ROOT/centos/5/iso
COSTAG=$HTTP_COSDIR/CentOS_BuildTag

case $1 in
#*******************************************************************************
   setup)
#*******************************************************************************
if test ! -f $COSTAG; then
   echo "CentOS-5 is not setup."
   exit 1;
fi
echo "Done."
;;
#*******************************************************************************
   cleanup)
#*******************************************************************************
echo "Done."
;;
#*******************************************************************************
   *)
#*******************************************************************************
echo "Usage:"
echo "   ./touchstone-iqp-1.0.0.init.sh setup|cleanup"
;;
esac

