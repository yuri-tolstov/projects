#!/bin/bash
#-------------------------------------------------------------------------------
# Array
#-------------------------------------------------------------------------------
ARRAY=(apple orange tomato)
ARRAY[3]=grape
echo "All.1: ${ARRAY[*]}"
echo "All.2: ${ARRAY[@]}"
echo "0: ${ARRAY[0]}"
echo "1: ${ARRAY[1]}"
echo "2: ${ARRAY[2]}"
echo "3: ${ARRAY[3]}"

echo "Scannig the array..."
for ii in "${ARRAY[@]}"; do
   echo "$ii"
done

#-------------------------------------------------------------------------------
# Associative array
#-------------------------------------------------------------------------------
declare -A AARRAY
AARRAY[key1]="value1"
AARRAY[key2]="value2"
AARRAY[key3]="value3"
echo "All: ${AARRAY[@]}"

echo "Scannig the a-array..."
for ii in "${AARRAY[@]}"; do
   echo "$ii"
done

echo "Key1: ${AARRAY[key1]}"
echo "Key2: ${AARRAY[key2]}"
echo "Key3: ${AARRAY[key3]}"

echo "Scannig the a-array different way..."
KARRAY=(key1 key2 key3)
for kk in "${KARRAY[@]}"; do
   echo "$kk: ${AARRAY[$kk]}"
done

#-------------------------------------------------------------------------------
# Practical example
#-------------------------------------------------------------------------------
echo "Practical example.."
BOARDS=(n804 n805 n808)
declare -A CPUS
CPUS[n804]="CN6645"
CPUS[n805]="CN6335"
CPUS[n808]="CN6645"

for ii in "${BOARDS[@]}"; do
   echo "$ii: ${CPUS[$ii]}"
done



