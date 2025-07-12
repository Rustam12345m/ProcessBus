#!/bin/sh
set -e

SCRIPT_PATH="$(dirname "$(realpath "$0")")"
REPO_DIR="$SCRIPT_PATH/../../"
BUILD_DIR="$REPO_DIR/build/"
INSTALL_DIR="$REPO_DIR/install/"

USER=user
HOST=192.168.21.2
REMOTE_DIR=/home/user/pbus/

$REPO_DIR/ci/build.sh --rebuild

sshpass -p 123 scp -r "$INSTALL_DIR/bin/" "${USER}@${HOST}:${REMOTE_DIR}"

