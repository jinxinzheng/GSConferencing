#!/bin/awk -f

$1=="table"{
  t=1
  table=$2
  write_format=""
  write_args=""
  read_statements=""
  next
}

t&&$1=="i"{
  write_format=write_format "%d:"
  write_args=write_args "d->" $2 ","
  read_statements=read_statements "d->" $2 "=atoi(shift(c)); "
}

t&&$1=="s"{
  write_format=write_format "%s:"
  write_args=write_args "d->" $2 ","
  read_statements=read_statements "strcpy(d->" $2 ",shift(c)); "
}

t&&/^$/{
# flush table
  t=0
  write_format=substr(write_format, 0, length(write_format)-1) "\\n"
  write_args=substr(write_args, 0, length(write_args)-1)
  printf "#define write_%s(d){ append(\"%s\", %s); }\n", table, write_format, write_args
  printf "#define read_%s(d,c){ a=1; %s }\n\n", table, read_statements
}
