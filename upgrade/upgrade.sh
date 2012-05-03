#!/bin/sh

server_dir="/"
server_exe="daya-sys-arm-linux"

cur_ver="$1"
server_pid="$2"

[ -f server.version ] || exit 1

new_ver=`cat server.version`

if [ "$cur_ver" != "$new_ver" ]; then
  rm -rf server_temp
  mkdir -p server_temp
  tar -C server_temp -jxf server.bz2 || exit 2
  kill -9 $server_pid
  mv server_temp/* $server_dir/
  cd $server_dir
  chmod +x $server_exe
  ./$server_exe &
fi
