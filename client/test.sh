#!/bin/sh

./test-client -i 801 &
./test-client -i 901 &

sleep 2

./test-client -i 101 &
./test-client -i 102 &
./test-client -i 103 -s 8 &
