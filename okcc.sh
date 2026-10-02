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
ENTRY_ADDRESS=$(nm -n "$WORK_DIR/program.elf" | awk '$3 == "_start" { print $1; exit }')
if [ -z "$ENTRY_ADDRESS" ]; then
    echo "error: linked userspace program has no _start entry point" >&2
    exit 1
fi
ENTRY_OFFSET=$((0x$ENTRY_ADDRESS - 0x200000))

python3 - "$WORK_DIR/program.bin" "$OUTPUT" "$ENTRY_OFFSET" <<'PY'
import struct
import sys

payload = open(sys.argv[1], "rb").read()
entry_offset = int(sys.argv[3], 10)
if entry_offset < 0 or entry_offset >= len(payload):
    raise SystemExit("error: _start is outside the packaged userspace image")
header = struct.pack("<4sIII", b"OKX1", 0x200000, entry_offset, len(payload))
with open(sys.argv[2], "wb") as output:
    output.write(header)
    output.write(payload)
PY