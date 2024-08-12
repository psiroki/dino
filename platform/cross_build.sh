#!/bin/bash

# platforms.txt should have lines in the following format:
# MIYOO mm /path/to/dir/containing/armhf/cross/compile/docker/makefile
# (so the parent directory of the Makefile, not the path to the Makefile itself)

SCRIPT_DIR="$(dirname $0)"
PROJECT_ROOT="$(realpath "$SCRIPT_DIR/..")"

while read -r line && [ -n "$line" ]; do
  TAG="$(echo $line | cut -d ' ' -f 1)"
  SUFFIX="$(echo $line | cut -d ' ' -f 2)"
  TCD="$(realpath $(echo $line | cut -d ' ' -f 3-))"
  cd "$PROJECT_ROOT"
  mkdir -p build/platforms/$SUFFIX
  DIR="$PROJECT_ROOT/build/platforms/$SUFFIX"
  if [ "$TAG" == "WEB" ]; then
    cd "$DIR"
    cmake "$PROJECT_ROOT" -DCMAKE_TOOLCHAIN_FILE="$TCD"
    make
  else
    cd "$TCD"
    make "WORKSPACE_DIR=$PROJECT_ROOT" "DOCKER_CMD=/bin/bash /workspace/platform/internal_build_helper.sh build/platforms/$SUFFIX $TAG" "INTERACTIVE=0" shell
  fi
done < "$SCRIPT_DIR/platforms.txt"
