#!/bin/bash
service networking stop
sleep 2
rmmod octeon-ethernet
dmesg -c
insmod ./octeon-ethernet.ko aneg_mode=1
sleep 3
service networking start
