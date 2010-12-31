#!/bin/bash

n=100
[ $1 ] && n=$1

for ((i=1000; i<1000+n; i++)); do
  xterm -geometry 20x15 -e ./test-client -a 127.0.0.1 -i $i -s 1 -v &
done
