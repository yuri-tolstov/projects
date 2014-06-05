#!/bin/bash
INIDIR=$(pwd)
SDKDIR=$INIDIR/usr/local/Cavium_Networks/OCTEON-SDK
#-------------------------------------------------------------------------------
# Verify the environment
#-------------------------------------------------------------------------------
SDKFLIST="OCTEON-SDK-3.1.0-515.i386.rpm
          OCTEON-LINUX-3.1.0-515.i386.rpm
          sdk_3.1.0_update_p2.tgz"
for file in $SDKFLIST; do
   if test ! -e $file; then
      echo "Missing $file"
      exit 1
   fi
done
#-------------------------------------------------------------------------------
# Install SDK from RPMs
#-------------------------------------------------------------------------------
rpm2cpio OCTEON-SDK-3.1.0-515.i386.rpm | cpio -vid
rpm2cpio OCTEON-LINUX-3.1.0-515.i386.rpm | cpio -vid
cp sdk_3.1.0_update_p2.tgz $SDKDIR

cd $SDKDIR
tar xzvf sdk_3.1.0_update_p2.tgz
patch -s -p1 < sdk_3.1.0_patch_release2/sdk.patch
patch -s -p1 < sdk_3.1.0_patch_release2/linux.patch
sdk_3.1.0_patch_release2/octeon_copy_3_1_0_p2_files.sh sdk_3.1.0_patch_release2
tar xzvf tools-gcc-4.7.tgz

rm -f tools-gcc-4.7.tgz
rm -f sdk_3.1.0_update_p2.tgz
rm -rf sdk_3.1.0_patch_release2

#-------------------------------------------------------------------------------
# Cleanup
#-------------------------------------------------------------------------------
source env-setup OCTEON_CN66XX
make -C bootloader/u-boot distclean
make -C linux clean
make -C linux/embedded_rootfs clean
make -C host/remote-lib clean
make -C host/remote-utils clean

for dir in $(ls -p examples | grep "/"); do
   make -C examples/$dir clean
done

#-------------------------------------------------------------------------------
# Modify directories and files
#-------------------------------------------------------------------------------
LDOCDIR=linux/kernel/linux/Documentation
mv $LDOCDIR docs/linux
mkdir -p $LDOCDIR/DocBook
touch $LDOCDIR/Makefile
touch $LDOCDIR/DocBook/Makefile
echo "Moved to OCTEON_ROOT/docs/linux directory" > $LDOCDIR/README

#-------------------------------------------------------------------------------
# Create Git repositories
#-------------------------------------------------------------------------------
GITBREPOD=$INIDIR/gitbrepo
GITBREPOS="octeon-sdk
           octeon-sdk.tools
           octeon-sdk.executive
           octeon-sdk.bootloader
           octeon-sdk.linux
           octeon-sdk.docs"

for repo in $GITBREPOS; do
   git init --bare $GITBREPOD/$repo.git
done

REPOLIST="docs executive bootloader linux tools"
for repo in $REPOLIST; do
   cd $SDKDIR/$repo
   git init
   git add .
   git commit -m "Initial import from OCTEON SDK-3.1.0-P2"
   git tag -a OCTEON-SDK-3.1.0-515-P2 -m "SDK-3.1.0-515 + Patch2 from Cavium"
   git branch vendor
   git remote add origin file:///$GITBREPOD/octeon-sdk.$repo.git
   git push --all -u
done

FDLIST="diagnostic ejtag examples host licenses simulator target application.mk common-config.mk common.mk env-setup env-setup.pl executive_lib.mk octeon-models.txt README.txt release-notes.txt"
cd $SDKDIR
git init
git add $FDLIST
git commit -m "Initial import from OCTEON SDK-3.1.0-P2"
git tag -a OCTEON-SDK-3.1.0-515-P2 -m "SDK-3.1.0-515 + Patch2 from Cavium"
git branch vendor
git remote add origin file:///$GITBREPOD/octeon-sdk.git
git push --all -u

