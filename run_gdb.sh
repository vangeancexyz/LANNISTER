#!/bin/bash
PID=$(pidof cstrike_linux64)

if [ -z "$PID" ]; then
    echo "Error: cstrike_linux64 is not running."
    exit 1
fi

LIB_PATH="$(pwd)/lannister.so"

echo "Injecting via GDB into PID: $PID"
echo "Lib: $LIB_PATH"

sudo gdb -batch-silent -p $PID -ex "print (void*)dlopen(\"$LIB_PATH\", 2)" -ex "quit"

echo "Success"
