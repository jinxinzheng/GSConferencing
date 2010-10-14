#!/bin/sh

./test-client -i 101 &
./test-client -i 102 -s 9 &
./test-client -i 103 -s 8 &

sleep 2

./test-client -i 801 &
./test-client -i 901 &
