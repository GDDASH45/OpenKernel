// metadata

#include <kernel/metadata.h>

const char *metadata_marker = "start_metadata";

const char *os_version = "(OpenKernel 0.0.1-rc1.2)";

const char *kernel_name = "OpenKernel";
const char *kernel_version = "0.0.1-rc1.2";
const char *kernel_banner = "OpenKernel v0.0.1 | x86 Bare Metal\n";

const char *msg_boot_ok = "[OK] System booted successfully.\n";
const char *msg_init_tar = "[FS] Parsing TAR initramfs...\n";
const char *msg_panic_default = "Unhandled exception or fatal system error.";

const char *shell_prompt = "ok> ";

// Additional system, driver, and shell messages
const char *msg_init_vfs = "[FS] Initializing Virtual File System...\n";
const char *msg_init_fb = "[GFX] Setting up linear framebuffer...\n";
const char *msg_init_kbd = "[KBD] Registering PS/2 keyboard handler...\n";
const char *msg_init_spkr = "[SND] Initializing PC speaker driver...\n";

const char *msg_tar_found = "[TAR] File located in archive: ";
const char *msg_tar_missing = "[TAR] Error: File not found in initramfs.\n";
const char *msg_tar_invalid = "[TAR] Error: Invalid archive format or magic.\n";

const char *msg_pmx_loading = "[PMX] Loading flat binary into memory...\n";
const char *msg_pmx_bad_magic = "[PMX] Error: Invalid executable magic (expected PMX1).\n";
const char *msg_pmx_overflow = "[PMX] Error: Code segment exceeds binary bounds.\n";
const char *msg_pmx_exec = "[PMX] Jumping to program entry point...\n";

const char *msg_osf_playing = "[OSF] Playing OpenKernel Sound Format sequence...\n";
const char *msg_osf_bad = "[OSF] Error: Malformed sound sequence header.\n";

const char *shell_help_text =
"Available commands:\n"
"  help       - Display this help message\n"
"  ls         - List files in initramfs\n"
"  play - Play an OSF sound file\n"
"  exec - Execute a PMX binary\n"
"  clear      - Clear the screen\n"
"  reboot     - Reset the system\n";

const char *shell_err_cmd = "Unknown command. Type 'help' for available commands.\n";
const char *shell_err_args = "Error: Missing or invalid arguments.\n";
const char *shell_msg_reboot = "Rebooting system...\n";
const char *shell_msg_clear = "\033[2J";