#!/bin/bash
#*******************************************************************************
# Functions
#*******************************************************************************
function check_arguments() {
   local destdir=$1
   if test "$destdir" == ""; then
      echo "Missing destination directory"
      exit 1
   fi
}
#*******************************************************************************
# Program
#*******************************************************************************
UCMD=$1
TGTDIR=$2

read -p "User name: " USER
read -s -p "User password: " PASS

SREPOURL=https://$USER:$PASS@50.0.45.248/git/unified

case $UCMD in
#-------------------------------------------------------------------------------
   bootloader)
#-------------------------------------------------------------------------------
echo "Cloning OCETON SDK bootloader set..."
SMODLIST="tools executive bootloader"
CLONESDK=1
;;
#-------------------------------------------------------------------------------
   executive)
#-------------------------------------------------------------------------------
echo "Cloning OCETON SDK executive set..."
SMODLIST="tools executive"
CLONESDK=2
;;
#-------------------------------------------------------------------------------
   linux)
#-------------------------------------------------------------------------------
echo "Cloning OCETON SDK linux set..."
SMODLIST="tools executive linux"
CLONESDK=2
;;
#-------------------------------------------------------------------------------
   docs)
#-------------------------------------------------------------------------------
check_arguments $TGTDIR
echo "Cloning OCETON SDK docs set..."
SMODLIST="docs"
CLONESDK=3
;;
#-------------------------------------------------------------------------------
   full)
#-------------------------------------------------------------------------------
echo "Cloning OCETON SDK full set..."
SMODLIST="tools executive bootloader linux docs"
CLONESDK=4
;;
#-------------------------------------------------------------------------------
   --help | *)
#-------------------------------------------------------------------------------
echo "Usage:
   octeon-sdk-clone.sh <sdk-set> <target-dir>
Arguments:
   sdk-set:
      bootloader | executive | linux | docs | full
   target-repo-url:
      Target directory name (e.g. octeon-sdk.bootloader)
"
exit 0
;;
esac

#-------------------------------------------------------------------------------
# Clone the SDK
#-------------------------------------------------------------------------------
if test $CLONESDK -gt 0; then
   check_arguments $TGTDIR
   mkdir -p $TGTDIR
   cd $TGTDIR
   git clone $SREPOURL/octeon-sdk.git .
   if test $? -ne 0; then
      echo "Failed to access the OCTEON SDK Git repository"
      exit 2
   fi
   for smod in $SMODLIST; do
      git submodule add $SREPOURL/octeon-sdk.$smod $smod
   done
fi

