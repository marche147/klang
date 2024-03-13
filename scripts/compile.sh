#!/bin/bash

set -e

SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )
RUNTIME_DIR="$SCRIPT_DIR/runtime"
COMPILER="$SCRIPT_DIR/klang"

if [ $# -lt 2 ]; then
    echo "Usage: compile.sh <source_file> <out>"
    exit 1
fi

SOURCE_FILE=$1
EXE=$2
OUT=$(mktemp --suffix .S)

$COMPILER $SOURCE_FILE $OUT
if [ $? -ne 0 ]; then
    exit 1
fi

gcc -o $EXE $OUT $RUNTIME_DIR/*
if [ $? -ne 0 ]; then
    exit 1
fi