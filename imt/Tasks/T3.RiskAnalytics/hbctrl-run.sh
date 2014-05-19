#!/bin/bash
#==========================================================
# Functions
#==========================================================
function nifstat() {
   local if2stat
   local if3stat
   local if2rxc
   local if3rxc
   local count=2 

   while test $count -gt 0; do
      sleep 10
      if2stat=$(ethtool eth2 | awk '/Link/ {print $3}')
      if3stat=$(ethtool eth3 | awk '/Link/ {print $3}')
      if2rxc=$(ifconfig eth2 | awk '/RX\ packets/ {split($2, rxc, ":"); print rxc[2]}')
      if3rxc=$(ifconfig eth3 | awk '/RX\ packets/ {split($2, rxc, ":"); print rxc[2]}')
      echo "eth2 = ($if2stat, $if2rxc) eth3 = ($if3stat, $if3rxc)"
      ((count--))
   done
}

#==========================================================
# Program
#==========================================================
ifconfig eth2 promisc up
ifconfig eth3 promisc up
GCOUNT=10000
while test $GCOUNT -gt 0; do
   echo "---------- Cycle $GCOUNT"
   ./hbtool set --segment=1 --current-mode=4
   echo "Bypass is on"
   nifstat
   ./hbtool set --segment=1 --current-mode=2
   echo "Bypass is off"
   nifstat
   ((GCOUNT--))
done

