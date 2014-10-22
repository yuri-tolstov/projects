#!/bin/sh

readresponse()
{
echo -n "${1} [Y/N]? "
read response
if [ "${response}" = "y" ]
 then
 response="Y"
elif [ "${response}" = "n" ]
 then
 response="N"
fi
}

response=""
for i in /sys/class/block/sd* ; do
  if [ "${response}" != "Y" \
       -a -f "${i}/removable" \
       -a -f "${i}/device/vendor" \
       -a -f "${i}/device/model" ]
    then
    removable=`cat "${i}/removable"`
    size=`cat "${i}/size"`
    vendor=`cat "${i}/device/vendor" | sed -e 's/^ *//' -e 's/ *$//'`
    model=`cat "${i}/device/model" | sed -e 's/^ *//' -e 's/ *$//'`
    if [ "${removable}" = "1" -a "${size}" != "0" ]
      then
      dev=`cat "${i}"/dev`
      devicefile="/dev/"`ls -l --time-style="+" /dev \
        | egrep ^b | sed -e 's/  */ /g' \
        | cut -d' ' -f5- | sed 's/, /:/' \
        | fgrep "${dev}" | cut -d' ' -f2`
      echo "Found ${vendor} ${model}: ${devicefile}"

      response=""
      while [ "${response}" != "Y" -a "${response}" != "N" ]
        do
        readresponse "Format ${devicefile} and install bootable system on it"
      done
    fi
  fi
done
if [ "${response}" != "Y" -a "${response}" != "N" ]
  then
  echo "No suitable devices found"
  exit 1
fi
if [ "${response}" != "Y" ]
  then
  echo "Exiting"
  exit 1
fi
readresponse "Really format ${devicefile}"
if [ "${response}" != "Y" ]
  then
  echo "Exting"
  exit 1
fi
echo "Replacing partition table"
parted -s "${devicefile}" "mklabel msdos" "mkpart primary fat32 1s 2s" "mkpart primary fat32 1c -1s" "rm 1" "print"
echo "Re-reading partition table"
blockdev --rereadpt "${devicefile}"
echo "Creating filesystem"
mkfs.msdos "${devicefile}"2
echo "Sync"
sync
echo "Copying X-Loader"
dd if=xload.bin seek=2 of="${devicefile}"
echo "Sync"
sync
rmdir tmp-mnt 2>/dev/null
mkdir tmp-mnt
echo "Mounting filesystem"
until mount "${devicefile}"2 tmp-mnt
do sleep 1
done
echo "Installing U-Boot"
# cp u-boot.bin xload.bin vmlinux im13029-linux.dtb im13029-linux-flash.dtb tmp-mnt/
cp u-boot.bin xload.bin tmp-mnt/
sync
echo "Unmounting filesystem"
until umount tmp-mnt
do sleep 1
done
rmdir tmp-mnt
echo "Sync"
sync
echo "Finished"

