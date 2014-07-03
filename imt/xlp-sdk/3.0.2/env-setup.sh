#!/bin/bash

SDK_ROOT=$(pwd)/output/build
SYSCONFIG_DIR=${SDK_ROOT}/sysconfig
TOOLCHAINS_DIR=${SDK_ROOT}/toolchains_bin
BRCM_LIB_DIR=${SDK_ROOT}/libraries
HAL_DIR=${BRCM_LIB_DIR}/hal
FDT_DIR=${BRCM_LIB_DIR}/fdt
TARGET_DIR=${SDK_ROOT}/target
BIN_DIR=${TARGET_DIR}/mips64/bin
PATH=${TOOLCHAINS_DIR}/mipscross/elf/bin:${TOOLCHAINS_DIR}/mipscross/linux/bin:${PATH}
export PATH SDK_ROOT BRCM_LIB_DIR HAL_DIR FDT_DIR BIN_DIR

ARCH=mips64
API=${ABI:=64}
LE=${LE:=0}
BUILDTYPE=XLPEV_N511
BUILDMODE=cross
source ${TOOLCHAINS_DIR}/toolchain_setup.sh
export CROSS_COMPILE=mips64-nlm-linux-
export ARCH LE BUILDTYPE BUILDMODE CROSS_COMPILE

#===============================================================================
# Build X-Loader
#===============================================================================
# cd $SDK_ROOT/xloader
# make distclean
# make netl8xx_N511_config
# make	=> Generates xload.bin

#===============================================================================
# Build U-Boot
#===============================================================================
# cd $SDK_ROOT/xloader
# make distclean
# make netl8xx_N511_config
# make	=> Generates uboot.bin

