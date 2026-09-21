#!/bin/sh
set -eu

if [ "$#" -ne 2 ]; then
    echo "usage: $0 input.c output.okx" >&2
    exit 2
fi

INPUT=$1
OUTPUT=$2
WORK_DIR=$(mktemp -d)
trap 'rm -rf "$WORK_DIR"' EXIT

gcc -m32 -ffreestanding -fno-pie -fno-stack-protector -nostdlib \
    -Iinclude -c "$INPUT" -o "$WORK_DIR/program.o"
ld -m elf_i386 -Ttext=0x200000 --entry=_start \
    -o "$WORK_DIR/program.elf" "$WORK_DIR/program.o"
objcopy -O binary "$WORK_DIR/program.elf" "$WORK_DIR/program.bin"

python3 - "$WORK_DIR/program.bin" "$OUTPUT" <<'PY'
import struct
import sys

payload = open(sys.argv[1], "rb").read()
header = struct.pack("<4sIII", b"OKX1", 0x200000, 0, len(payload))
with open(sys.argv[2], "wb") as output:
    output.write(header)
    output.write(payload)
PY