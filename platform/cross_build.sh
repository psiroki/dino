#!/bin/bash

# platforms.txt should have lines in the following format:
# MIYOO mm /path/to/armhf/cross/compile/docker/makefile

SCRIPT_DIR="$(dirname $0)"
PROJECT_ROOT="$(realpath "$SCRIPT_DIR/..")"

while read -r line && [ -n "$line" ]; do
  TAG="$(echo $line | cut -d ' ' -f 1)"
  SUFFIX="$(echo $line | cut -d ' ' -f 2)"
  TCD="$(realpath $(echo $line | cut -d ' ' -f 3-))"
  echo $TAG
  echo $PROJECT_ROOT/build-$SUFFIX
  echo $TCD
  cd "$PROJECT_ROOT"
  mkdir -p build-$SUFFIX
  DIR="$PROJECT_ROOT/build-$SUFFIX"
  cd "$TCD"
  echo "DOCKER_CMD=/bin/bash /workspace/platform/platform_build.sh build-$SUFFIX $TAG"
  make "WORKSPACE_DIR=$PROJECT_ROOT" "DOCKER_CMD=/bin/bash /workspace/platform/platform_build.sh build-$SUFFIX $TAG" "INTERACTIVE=0" shell
done < "$SCRIPT_DIR/platforms.txt"
