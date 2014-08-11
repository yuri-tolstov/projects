#!/bin/bash

RELNAME=hbtool
VERMAJ=$(awk '/HBTOOL_VERSION_MAJOR/ {print $3}' version.h)
VERMIN=$(awk '/HBTOOL_VERSION_MINOR/ {print $3}' version.h)
VERREL=$(awk '/HBTOOL_VERSION_RELNUM/ {print $3}' version.h)
BLDNUM=$(awk '/HBTOOL_VERSION_BLDNUM/ {print $3}' version.h)
RELARC=$RELNAME-$VERMAJ.$VERMIN.$VERREL-$BLDNUM.src.tgz

FILIST="Makefile version.h niagara.h main.c n804.c n808.c"

tar czvf $RELARC $FILIST > /dev/zero
echo "Generated $RELARC"

