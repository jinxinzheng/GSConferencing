#!/bin/sh

arcfile=upgrade.bin

[ -f "$arcfile" ] || exit 1

rm -rf temp
mkdir -p temp
tar -C temp -xf "$arcfile" || exit 3

cd temp

# check files exist and sums
for f in *.bz2; do
  name=${f%.bz2}
  [ -f $name.md5 ] || exit 1
  [ -f $name.version ] || exit 1
  sum1=`cat $name.md5 | awk '{print $1}'`
  sum2=`md5sum $f | awk '{print $1}'`
  [ "$sum1" = "$sum2" ] || exit 2
done
