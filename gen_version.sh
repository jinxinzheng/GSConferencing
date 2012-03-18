#!/bin/bash

if git --version && test -d .git; then
  ver="`git describe --abbrev=4 HEAD`" \
  || ver=`git rev-parse --short HEAD`
else
  ver=unknown
fi

date="`date +'%F %T'`"

cat >.version.c <<EOF
const char *build_date = "$date";
const char *build_rev = "$ver";
EOF
