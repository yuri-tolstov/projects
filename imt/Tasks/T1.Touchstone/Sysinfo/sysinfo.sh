#!/bin/bash
OFILE=sysinfo.out
echo "Collecting System configuration..."
echo -e "\n**************** uname -a **************************" > $OFILE
cat /etc/redhat-release >> $OFILE
uname -a >> $OFILE

echo -e "\n**************** sysctl -a **************************" >> $OFILE
sysctl -a >> $OFILE

echo -e "\n\n**************** ulimit -a **************************" >> $OFILE
ulimit -a >> $OFILE

echo -e "\n**************** rpm -qa **************************" >> $OFILE
rpm -qa >> $OFILE

echo -e "\n**************** dmidecode **************************" >> $OFILE
dmidecode >> $OFILE

echo -e "\n**************** dmesg **************************" >> $OFILE
dmesg >> $OFILE

echo -e "\n**************** ps -e **************************" >> $OFILE
ps -e >> $OFILE

echo -e "\n**************** cat /proc/cpuinfo **************************" >> $OFILE
cat /proc/cpuinfo >> $OFILE

echo -e "\n**************** cat /proc/meminfo **************************" >> $OFILE
cat /proc/meminfo >> $OFILE

echo -e "\n**************** cat /proc/modules **************************" >> $OFILE
cat /proc/modules >> $OFILE

echo -e "\n**************** lsmod **************************" >> $OFILE
lsmod >> $OFILE

echo -e "\n**************** ethtool -i ethX **************************" >> $OFILE
NIFS=$(ls /sys/class/net | grep eth)
for nif in $NIFS; do
   echo "-----------------------" >> $OFILE
   ethtool $nif >> $OFILE
   ethtool -i $nif >> $OFILE 2>&1
   ethtool -g $nif >> $OFILE 2>&1
   ethtool -c $nif >> $OFILE 2>&1
   ethtool -k $nif >> $OFILE 2>&1
   ethtool -a $nif >> $OFILE 2>&1
done

echo -e "\n**************** Network Monitoring ************************" >> $OFILE
echo "Network Monitoring..."
COUNT=10
while test $COUNT -gt 0; do
   echo "---------------- Cycle $COUNT" >> $OFILE
   for nif in $NIFS; do
      ifconfig $nif >> $OFILE
      ethtool -S $nif >> $OFILE
   done
   netstat -i >> $OFILE
   netstat -s >> $OFILE
   sleep 1
   ((COUNT--))
done

echo -e "\n**************** Interrupt Monitoring ************************" >> $OFILE
echo "Interrupt Monitoring..."
COUNT=10
while test $COUNT -gt 0; do
   echo "---------------- Cycle $COUNT" >> $OFILE
   cat /proc/interrupts >> $OFILE
   sleep 1
   ((COUNT--))
done
echo "Done."

