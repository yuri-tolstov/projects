#!/bin/bash
RELDIR=$(pwd)
ISOFILE=CentOS-6.5-x86_64-bin-DVD1.iso
HTTP_ROOT=/var/www/doc
HTTP_ISODIR=$RELDIR/iso

TFTP_ROOT=/var/lib/tftpboot
TFTP_RELDIR=$TFTP_ROOT/centos6

LOCK=$HTTP_ISODIR/CentOS_BuildTag

case $1 in
#+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
   setup)
#+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
if test -f $LOCK; then
   echo "Already installed."
   exit 0;
fi
#-------------------------------------------------------------------------------
# TFTP
#-------------------------------------------------------------------------------
mkdir -p $TFTP_RELDIR
#-------------------------------------------------------------------------------
# HTTP
#-------------------------------------------------------------------------------
mkdir -p $HTTP_ISODIR
mount -t iso9660 -o loop $RELDIR/$ISOFILE $HTTP_ISODIR > /dev/zero
if test $? -eq 0; then
   echo "CentOS ISO is mounted in $HTTP_ISODIR"
else
   echo "Failed to mount CentOS ISO"
   exit 2
fi
cp $HTTP_ISODIR/images/pxeboot/vmlinuz $TFTP_RELDIR
cp $HTTP_ISODIR/images/pxeboot/initrd.img $TFTP_RELDIR
echo "Done."
;;
#+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
   cleanup)
#+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
sync
umount $HTTP_ISODIR
if test $? -ne 0; then
   echo "Failed to unmount $HTTP_ISODIR"
fi
rm -rf $HTTP_ISODIR
rm -rf $TFTP_RELDIR
echo "Done."
;;
#+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
   *)
#+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
echo "Usage:"
echo "  ./centos-dist-init.sh setup|cleanup"
echo "  After setup or cleanup, manually modify PXE configuration file -- /var/lib/tftpboot/pxelinux.cfg/default"
;;
esac

