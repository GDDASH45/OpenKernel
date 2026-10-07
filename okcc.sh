#!/bin/sh
set -eu

if [ "$#" -ne 2 ]; then
    echo "usage: $0 input.c output.okx|output.oso" >&2
    exit 2
fi

INPUT=$1
OUTPUT=$2
WORK_DIR=$(mktemp -d)
trap 'rm -rf "$WORK_DIR"' EXIT

case "$OUTPUT" in
    *.oso)
        LOAD_ADDR=0x300000
        ENTRY_SYMBOL=_oso_entry
        MAGIC=OSO1
        ;;
    *)
        LOAD_ADDR=0x200000
        ENTRY_SYMBOL=_start
        MAGIC=OKX1
        ;;
esac

gcc -m32 -ffreestanding -fno-pie -fno-stack-protector -nostdlib \
    -Iinclude -c "$INPUT" -o "$WORK_DIR/program.o"
ld -m elf_i386 -Ttext="$LOAD_ADDR" --entry="$ENTRY_SYMBOL" \
    -o "$WORK_DIR/program.elf" "$WORK_DIR/program.o"
objcopy -O binary "$WORK_DIR/program.elf" "$WORK_DIR/program.bin"
ENTRY_ADDRESS=$(nm -n "$WORK_DIR/program.elf" | awk -v symbol="$ENTRY_SYMBOL" '$3 == symbol { print $1; exit }')
if [ -z "$ENTRY_ADDRESS" ]; then
    echo "error: input has no $ENTRY_SYMBOL entry point" >&2
    exit 1
fi
ENTRY_OFFSET=$ENTRY_ADDRESS

python3 - "$WORK_DIR/program.bin" "$OUTPUT" "$ENTRY_OFFSET" "$MAGIC" "$LOAD_ADDR" <<'PY'
import struct
import sys

payload = open(sys.argv[1], "rb").read()
entry_offset = int(sys.argv[3], 16)
magic = sys.argv[4].encode("ascii")
load_addr = int(sys.argv[5], 0)
entry_offset -= load_addr
if entry_offset < 0 or entry_offset >= len(payload):
    raise SystemExit("error: entry point is outside the packaged image")
if magic == b"OSO1" and (load_addr != 0x300000 or len(payload) > 0x100000):
    raise SystemExit("error: OSO images must fit in the 1 MiB region at 0x300000")
header = struct.pack("<4sIII", magic, load_addr, entry_offset, len(payload))
with open(sys.argv[2], "wb") as output:
    output.write(header)
    output.write(payload)
PY