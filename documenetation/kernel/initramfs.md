# Initramfs, TAR, and startup scripts

## Building the archive

`build_fs.sh` makes a USTAR archive from the contents of `initfs/` at
`iso/boot/initramfs.img`. GRUB supplies that archive as a Multiboot module.
The default init entry is `sys/init`; it is a binary in the checked-in
initramfs, so a text script is not required for normal boot.

At startup, TAR entries are indexed into the VFS tree. If ATA-backed OKFS
mounts successfully, regular files are also copied into that separate store.
The configured init is then looked up in the archive and run directly from
there. See [Boot and startup](boot.md) and [VFS and storage](storage.md).

## Archive support and constraints

The TAR reader handles USTAR-style headers and octal sizes. It recognizes
leading `/` and `./` when comparing entry names; VFS indexing also considers
the USTAR prefix field. The executable lookup path is based on the header name.
The parser does not validate checksums or receive an archive-length boundary,
so treat the initramfs as trusted build input. It is not a hardened parser for
untrusted or arbitrary TAR variants.

Boot additionally expects entries in the `boot/`, `root/`, and `device/`
namespace. Missing required entries panic before init is executed.

## Text init-script language

If the selected init entry contains printable text, the kernel interprets it as
a small line-oriented script. Supported commands are:

| Command | Effect |
| --- | --- |
| `clear` | Clear the kernel display. |
| `print TEXT` | Print the text after the command. |
| `exec PATH` | Load and execute the named initramfs program. |

Blank lines are ignored. A comment line must begin with `#` in column zero.
Lines have a fixed 127-character content limit. There is no variable
expansion, quoting, conditionals, PATH search, or command chaining. Unknown
commands and failed `exec` stop script processing and fail startup. The parser
is implemented in `kernel/script.c`.

## Adding or selecting init

Place a program or script in `initfs/`, rebuild the archive, and choose its
archive path using `init=...` in the GRUB command line. Paths are archive entry
names, not paths looked up through the mounted VFS. `exec PATH` within a script
uses the same initramfs archive loader.
