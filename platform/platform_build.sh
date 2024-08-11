#!/bin/bash

set -e

if [ "$#" -ne 2 ]; then
  echo platform_build.sh '<build directory> <tag>' 1>&2
  exit
fi

echo Building in $1
echo Running with $2

PS1=anything_but_empty_string
. /etc/bash.bashrc

cd "$(dirname "$0")"
cd ../$1

cmake .. "-D$2=ON"
make
