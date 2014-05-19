#!/bin/bash
#*******************************************************************************
# Procedure of generating Linux Debian images and installing them on USB drive.
#*******************************************************************************
set -x
if test "$OCTEON_ROOT" == ""; then
   echo "OCTEON build environment is not setup"
   exit 1
fi
cd $OCTEON_ROOT/linux
#-------------------------------------------------------------------------------
# Full cleanup
#-------------------------------------------------------------------------------
make clean
make -C debian clean
make -C embedded_rootfs distclean
# This will setup default embedded_rootfs/.config file
make -C kernel distclean

#-------------------------------------------------------------------------------
# Build Kernel for Debian
#-------------------------------------------------------------------------------
make debian
# During configuration prompt:
# 1. Add Silicon Image SATA support 
# 2. Everything else -- use defaults

#-------------------------------------------------------------------------------
# Install Debian (Kernel + Root FS) on the Disk
#-------------------------------------------------------------------------------
# 1. Determine the Disk using "fdisk -l" command
# 2. Modify the Makefile
#    1.1. Remove check for Disk size.
vi debian/Makefile
# 3. Install Debian on the chosen Disk
make -C debian DISK=/dev/sdb compact-flash

