#!/bin/sh

arcfile=upgrade.bin

[ -f "$arcfile" ] || exit 1

rm -rf temp
mkdir -p temp
tar -C temp -xf "$arcfile" || exit 3

cd temp

# check file exists
[ -f server.md5 ] || exit 1
[ -f server.bz2 ] || exit 1
[ -f server.version ] || exit 1
[ -f client.md5 ] || exit 1
[ -f client.bz2 ] || exit 1
[ -f client.version ] || exit 1

# do sum checks
sum1=`cat server.md5 | awk '{print $1}'`
sum2=`md5sum server.bz2 | awk '{print $1}'`
[ "$sum1" = "$sum2" ] || exit 2

sum1=`cat client.md5 | awk '{print $1}'`
sum2=`md5sum client.bz2 | awk '{print $1}'`
[ "$sum1" = "$sum2" ] || exit 2
