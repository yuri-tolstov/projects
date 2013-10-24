#!/bin/sh
#-------------------------------------------------------------------------------
# Command-line arguments
#-------------------------------------------------------------------------------
OPMODE=$1
#-------------------------------------------------------------------------------
# Build Environment Setup
#-------------------------------------------------------------------------------
HOSTCPU=$(uname -p)
DS11PROJECT=tilegx
DS11NPUCARD=tilegx
DS11NPUCARDV=0
DS11NPUARC=tile
DS11NPUMOD=tile36
DS11ENV_ROOT=$(pwd)
cd ../..
DS11VIEW_ROOT=$(pwd)/sde
DS11MAIN_ROOT=$DS11VIEW_ROOT
while test ! -d lost+found; do
   if test -d ds11sde; then
      break;
   fi
   cd ..
done
if test -d lost+found; then
   echo "*** ERROR: Invalid DS11 SDE environment."
   exit 1
fi 
cd ds11sde
DS11HOME=$(pwd)
export DS11PROJECT DS11HOME DS11ENV_ROOT DS11VIEW_ROOT DS11MAIN_ROOT
export HOSTCPU DS11NPUCARD DS11NPUCARDV DS11NPUARC DS11NPUMOD

if test "$DS11SDE_ROOT" == ""; then
   echo "*** ERROR: DS11SDE_ROOT is not defined."
   exit 1
fi
if test ! $DS11BUILD_ROOT; then
   DS11BUILD_ROOT=$DS11VIEW_ROOT/build
   export DS11BUILD_ROOT
   mkdir -p $DS11BUILD_ROOT
fi
if test ! $DS11INSTALL_ROOT; then
   DS11INSTALL_ROOT=$DS11VIEW_ROOT/install
   export DS11INSTALL_ROOT
   mkdir -p $DS11INSTALL_ROOT
fi
#-------------------------------------------------------------------------------
# Tile MDE 4.2 Build Environment Setup
#-------------------------------------------------------------------------------
source $DS11ENV_ROOT/$DS11NPUCARD.toolset.sh
if test ! -f $SDK_TILE_BIN/$SDK_SETUP_PROG; then
   echo "*** ERROR: $SDK_SETUP_PROG SDK is not in $SDK_TILE_BIN"
   exit 2
fi
export SDK_TILE_ROOT SDK_TILE_VAR
echo "TILE SDK = $SDK_TILE_ROOT"
cd $SDK_TILE_BIN
eval `./$SDK_SETUP_PROG`

# if test ! -f .sdkinstalled; then
#    echo "*** ERROR: TILE SDK is not setup yet."
#    exit 3
# fi
# SDKLOCK=$(awk -F"=[ ]*" '/^TILE_MODEL/ {print $2}' .sdkinstalled)
# if test "$SDKLOCK" != "$TILEMOD"; then
#    echo "*** ERROR: SDK belongs to $SDKLOCK"
#    exit 4
# fi
#-------------------------------------------------------------------------------
# Start the Terminal Window
#-------------------------------------------------------------------------------
HOME=$DS11VIEW_ROOT
export HOME
if test "$OPMODE" != "automake"; then
   source $DS11SDE_ROOT/env/envsetup.sh
   USER=$(whoami)
   git config --global user.name "$USER"
   git config --global user.email "$USER@ditechnetworks.com"
   cd $DS11MAIN_ROOT
   bash -i
fi
