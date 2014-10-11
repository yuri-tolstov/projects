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
for ii in ${ADDRAY[@]}; do
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
for ii in "${AARRAY[@]}"; do
   echo "$ii"
done
echo "Key1: ${AARRAY[key1]}"
echo "Key2: ${AARRAY[key2]}"
echo "Key3: ${AARRAY[key3]}"

