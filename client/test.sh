#!/bin/sh

addr=192.168.1.3

./test-client -a $addr -i 101 &
./test-client -a $addr -i 102 -s 9 &
./test-client -a $addr -i 103 -s 8 &

sleep 2

./test-client -a $addr -i 801 &
./test-client -a $addr -i 901 &
