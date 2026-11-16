#include <fs/iso9660/iso9660.h>
#include <kernel/ports.h>
#include <kernel/ata.h>
#include <stdint.h>

#define ATAPI_DATA          0
#define ATAPI_ERROR         1
#define ATAPI_FEATURES      1
#define ATAPI_BYTE_COUNT_L  4
#define ATAPI_BYTE_COUNT_H  5
#define ATAPI_DRIVE         6
#define ATAPI_STATUS        7
#define ATAPI_COMMAND       7

#define ATAPI_STATUS_ERROR  0x01
#define ATAPI_STATUS_DRQ    0x08
#define ATAPI_STATUS_BUSY   0x80

#define ATAPI_CMD_IDENTIFY  0xA1
#define ATAPI_CMD_PACKET    0xA0
#define ATAPI_PACKET_READ12 0xA8

#define ATAPI_TIMEOUT       1000000
#define ISO_SECTOR_SIZE     2048
#define ISO_ROOT_RECORD     156
#define ISO_MAX_DEPTH       32
#define ISO_PATH_COMPONENT  1024

static int atapi_device = -1;
static uint16_t atapi_io_base;
static int iso_mounted;
static int iso_ata_image;
static uint32_t iso_image_start_lba;
static uint32_t iso_volume_sectors;
static uint32_t iso_root_lba;
static uint32_t iso_root_size;
static int iso_joliet;
static uint8_t iso_sector[ISO_SECTOR_SIZE];
static uint16_t atapi_identify_data[256];

static int atapi_read_block(uint32_t lba, void *buffer);

/* Read one ISO logical block from ATAPI media or a 2048-byte ATA image block. */
static int iso_read_block(uint32_t lba, void *buffer)
{
    if (!iso_ata_image)
        return atapi_read_block(lba, buffer);

    if (buffer == 0 || lba > 0xFFFFFFFFu / 4u ||
        iso_image_start_lba > 0xFFFFFFFFu - lba * 4u - 3u)
        return -1;

    for (uint32_t i = 0; i < 4; ++i) {
        if (ata_read_sector(iso_image_start_lba + lba * 4u + i,
                            (uint8_t *)buffer + i * 512u) != 0)
            return -1;
    }
    return 0;
}


static void atapi_insw(uint16_t port, uint16_t *buffer, uint32_t count)
{
    __asm__ volatile (
        "cld; rep insw"
        : "+D"(buffer), "+c"(count)
        : "d"(port)
        : "memory"
    );
}

static void atapi_outsw(uint16_t port, const uint16_t *buffer, uint32_t count)
{
    __asm__ volatile (
        "cld; rep outsw"
        : "+S"(buffer), "+c"(count)
        : "d"(port)
        : "memory"
    );
}

static uint8_t atapi_in(uint16_t reg)
{
    return port_byte_in((uint16_t)(atapi_io_base + reg));
}

static void atapi_out(uint16_t reg, uint8_t value)
{
    port_byte_out((uint16_t)(atapi_io_base + reg), value);
}

static void atapi_delay(void)
{
    (void)atapi_in(ATAPI_STATUS);
    (void)atapi_in(ATAPI_STATUS);
    (void)atapi_in(ATAPI_STATUS);
    (void)atapi_in(ATAPI_STATUS);
}

static int atapi_wait(uint8_t required)
{
    uint32_t timeout = ATAPI_TIMEOUT;

    while (timeout-- != 0) {
        uint8_t status = atapi_in(ATAPI_STATUS);

        if (status == 0x00 || status == 0xFF)
            return -1;

        if (status & ATAPI_STATUS_BUSY)
            continue;

        if (status & ATAPI_STATUS_ERROR) {
            (void)atapi_in(ATAPI_ERROR);
            return -1;
        }

        if ((status & required) == required)
            return 0;
    }

    return -1;
}

static void atapi_select(int device)
{
    atapi_out(ATAPI_DRIVE, (uint8_t)(0xA0 | (device ? 0x10 : 0)));
    atapi_delay();
}

/*
 * IDENTIFY PACKET DEVICE distinguishes ATAPI devices from ATA disks.
 * The identify payload is consumed so the device is left ready for commands.
 */
static int atapi_probe(uint16_t io_base, int device)
{
    atapi_io_base = io_base;
    atapi_select(device);

    if (atapi_in(ATAPI_STATUS) == 0x00)
        return -1;

    atapi_out(ATAPI_FEATURES, 0);
    atapi_out(ATAPI_COMMAND, ATAPI_CMD_IDENTIFY);

    if (atapi_wait(ATAPI_STATUS_DRQ) != 0)
        return -1;

    atapi_insw((uint16_t)(atapi_io_base + ATAPI_DATA), atapi_identify_data, 256);

    return 0;
}

/*
 * Issue one READ(12) packet and transfer exactly one 2048-byte block.
 */
static int atapi_read_block(uint32_t lba, void *buffer)
{
    uint8_t packet[12];
    uint32_t transferred = 0;
    uint32_t timeout;
    uint8_t status;
    int completed = 0;

    if (buffer == 0 || atapi_device < 0)
        return -1;

    packet[0] = ATAPI_PACKET_READ12;
    packet[1] = 0;
    packet[2] = (uint8_t)(lba >> 24);
    packet[3] = (uint8_t)(lba >> 16);
    packet[4] = (uint8_t)(lba >> 8);
    packet[5] = (uint8_t)lba;
    packet[6] = 0;
    packet[7] = 0;
    packet[8] = 0;
    packet[9] = 1;
    packet[10] = 0;
    packet[11] = 0;

    atapi_select(atapi_device);

    if (atapi_wait(0) != 0)
        return -1;

    atapi_out(ATAPI_FEATURES, 0);
    atapi_out(ATAPI_BYTE_COUNT_L, 0x00);
    atapi_out(ATAPI_BYTE_COUNT_H, 0x08);
    atapi_out(ATAPI_COMMAND, ATAPI_CMD_PACKET);

    if (atapi_wait(ATAPI_STATUS_DRQ) != 0)
        return -1;

    atapi_outsw((uint16_t)(atapi_io_base + ATAPI_DATA), (const uint16_t *)packet, 6);

    timeout = ATAPI_TIMEOUT;

    while (timeout-- != 0) {
        uint16_t byte_count;
        uint32_t words;

        status = atapi_in(ATAPI_STATUS);

        if (status == 0x00 || status == 0xFF)
            return -1;

        if (status & ATAPI_STATUS_BUSY)
            continue;

        if (status & ATAPI_STATUS_ERROR) {
            (void)atapi_in(ATAPI_ERROR);
            return -1;
        }

        if (!(status & ATAPI_STATUS_DRQ)) {
            completed = 1;
            break;
        }

        byte_count = (uint16_t)atapi_in(ATAPI_BYTE_COUNT_L);
        byte_count |= (uint16_t)atapi_in(ATAPI_BYTE_COUNT_H) << 8;

        if (byte_count == 0 || (byte_count & 1))
            return -1;

        words = byte_count / 2;

        if (transferred + byte_count > ISO_SECTOR_SIZE)
            return -1;

        atapi_insw(
            (uint16_t)(atapi_io_base + ATAPI_DATA),
            (uint16_t *)((uint8_t *)buffer + transferred),
            words
        );

        transferred += byte_count;
    }

    if (!completed || transferred != ISO_SECTOR_SIZE)
        return -1;

    return 0;
}

static uint16_t iso_le16(const uint8_t *p)
{
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

static uint32_t iso_le32(const uint8_t *p)
{
    return (uint32_t)p[0] |
           ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) |
           ((uint32_t)p[3] << 24);
}

static int iso_equal(const char *a, const char *b)
{
    while (*a && *b) {
        char ca = *a++;
        char cb = *b++;

        if (ca >= 'a' && ca <= 'z')
            ca = (char)(ca - 'a' + 'A');
        if (cb >= 'a' && cb <= 'z')
            cb = (char)(cb - 'a' + 'A');

        if (ca != cb)
            return 0;
    }

    return *a == 0 && *b == 0;
}

/*
 * ISO9660 identifiers may carry a ;version suffix. Joliet identifiers are
 * UCS-2BE (two bytes per character); the public path API uses UTF-8.
 */
static int iso_name_length(const uint8_t *record)
{
    int length = record[32];
    int i;

    if (length == 1 && (record[33] == 0 || record[33] == 1))
        return 0;

    if (iso_joliet) {
        if (length & 1)
            return -1;

        i = length - 2;
        while (i >= 0 && record[33 + i] == 0x00 &&
               record[34 + i] >= '0' && record[34 + i] <= '9')
            i -= 2;
        if (i >= 0 && record[33 + i] == 0x00 &&
            record[34 + i] == ';')
            length = i;

        if (length >= 2 && record[33 + length - 2] == 0x00 &&
            record[33 + length - 1] == '.')
            length -= 2;

        return length;
    }

    if (length >= 2 && record[33 + length - 2] == ';' &&
        record[33 + length - 1] >= '0' &&
        record[33 + length - 1] <= '9') {
        i = length - 1;
        while (i > 0 && record[33 + i - 1] >= '0' &&
               record[33 + i - 1] <= '9')
            --i;
        if (i > 0 && record[33 + i - 1] == ';')
            length = i - 1;
    }

    if (length > 0 && record[33 + length - 1] == '.')
        --length;

    return length;
}

static int iso_append_utf8(char *name, uint32_t capacity,
                           uint32_t *used, uint32_t codepoint)
{
    if (codepoint <= 0x7F) {
        if (*used + 1 >= capacity)
            return -1;
        name[(*used)++] = (char)codepoint;
    } else if (codepoint <= 0x7FF) {
        if (*used + 2 >= capacity)
            return -1;
        name[(*used)++] = (char)(0xC0 | (codepoint >> 6));
        name[(*used)++] = (char)(0x80 | (codepoint & 0x3F));
    } else if (codepoint <= 0xFFFF) {
        if (codepoint >= 0xD800 && codepoint <= 0xDFFF)
            return -1;
        if (*used + 3 >= capacity)
            return -1;
        name[(*used)++] = (char)(0xE0 | (codepoint >> 12));
        name[(*used)++] = (char)(0x80 | ((codepoint >> 6) & 0x3F));
        name[(*used)++] = (char)(0x80 | (codepoint & 0x3F));
    } else if (codepoint <= 0x10FFFF) {
        if (*used + 4 >= capacity)
            return -1;
        name[(*used)++] = (char)(0xF0 | (codepoint >> 18));
        name[(*used)++] = (char)(0x80 | ((codepoint >> 12) & 0x3F));
        name[(*used)++] = (char)(0x80 | ((codepoint >> 6) & 0x3F));
        name[(*used)++] = (char)(0x80 | (codepoint & 0x3F));
    } else {
        return -1;
    }

    return 0;
}

static int iso_record_name(const uint8_t *record, char *name, uint32_t capacity)
{
    int length = iso_name_length(record);
    uint32_t used = 0;
    int i;

    if (length <= 0)
        return -1;

    if (!iso_joliet) {
        if ((uint32_t)length + 1 > capacity)
            return -1;

        for (i = 0; i < length; ++i) {
            uint8_t c = record[33 + i];

            if (c < 0x20 || c > 0x7E)
                return -1;

            name[used++] = (char)c;
        }
    } else {
        for (i = 0; i < length; i += 2) {
            uint32_t codepoint =
                ((uint32_t)record[33 + i] << 8) | record[34 + i];

            if (codepoint >= 0xD800 && codepoint <= 0xDBFF) {
                uint32_t low;

                if (i + 3 >= length)
                    return -1;

                low = ((uint32_t)record[35 + i] << 8) | record[36 + i];
                if (low < 0xDC00 || low > 0xDFFF)
                    return -1;

                codepoint = 0x10000 +
                    ((codepoint - 0xD800) << 10) + (low - 0xDC00);
                i += 2;
            }

            if (iso_append_utf8(name, capacity, &used, codepoint) != 0)
                return -1;
        }
    }

    name[used] = 0;
    return (int)used;
}

/*
 * Walk one directory and find a single path component.
 */
static int iso_find_entry(
    uint32_t directory_lba,
    uint32_t directory_size,
    const char *wanted,
    uint32_t *extent,
    uint32_t *size,
    int *is_directory)
{
    uint32_t offset = 0;

    while (offset < directory_size) {
        uint32_t sector_lba = directory_lba + offset / ISO_SECTOR_SIZE;
        uint32_t sector_offset = offset % ISO_SECTOR_SIZE;
        uint8_t record_length;
        uint8_t *record;
        char name[ISO_PATH_COMPONENT];
        int name_length;

        if (iso_read_block(sector_lba, iso_sector) != 0)
            return -1;

        record = iso_sector + sector_offset;
        record_length = record[0];

        if (record_length == 0) {
            offset += ISO_SECTOR_SIZE - sector_offset;
            continue;
        }

        if (record_length < 34 ||
            sector_offset + record_length > ISO_SECTOR_SIZE ||
            offset + record_length > directory_size)
            return -1;

        name_length = iso_record_name(record, name, sizeof(name));

        if (name_length > 0 && iso_equal(name, wanted)) {
            *extent = iso_le32(record + 2);
            *size = iso_le32(record + 10);
            *is_directory = (record[25] & 0x02) != 0;

            if (*extent >= iso_volume_sectors ||
                (*size != 0 &&
                 ((*size - 1) / ISO_SECTOR_SIZE) >
                 iso_volume_sectors - *extent - 1))
                return -1;

            return 0;
        }

        offset += record_length;
    }

    return -1;
}

/*
 * Resolve an absolute ISO path. Returns the final extent and byte size.
 */
static int iso_resolve(
    const char *path,
    uint32_t *extent,
    uint32_t *size,
    int *is_directory)
{
    uint32_t current_lba = iso_root_lba;
    uint32_t current_size = iso_root_size;
    const char *p = path;
    unsigned int depth = 0;

    if (path == 0 || path[0] != '/')
        return -1;

    while (*p == '/')
        ++p;

    if (*p == 0) {
        *extent = iso_root_lba;
        *size = iso_root_size;
        *is_directory = 1;
        return 0;
    }

    while (*p) {
        char component[ISO_PATH_COMPONENT];
        uint32_t length = 0;
        uint32_t next_lba;
        uint32_t next_size;
        int next_is_directory;

        while (*p && *p != '/') {
            if (length + 1 >= sizeof(component))
                return -1;
            component[length++] = *p++;
        }

        component[length] = 0;

        while (*p == '/')
            ++p;

        if (length == 0 || ++depth > ISO_MAX_DEPTH)
            return -1;

        if (iso_find_entry(
                current_lba,
                current_size,
                component,
                &next_lba,
                &next_size,
                &next_is_directory) != 0)
            return -1;

        if (*p && !next_is_directory)
            return -1;

        current_lba = next_lba;
        current_size = next_size;
        *is_directory = next_is_directory;
    }

    *extent = current_lba;
    *size = current_size;
    return 0;
}

int iso9660_detect_device(void)
{
    static const uint16_t channels[] = { 0x1F0, 0x170 };

    atapi_device = -1;
    for (int channel = 0; channel < 2; ++channel) {
        for (int device = 0; device < 2; ++device) {
            if (atapi_probe(channels[channel], device) == 0) {
                atapi_device = device;
                return 0;
            }
        }
    }

    return -1;
}

int iso9660_read_device_block(uint32_t lba, void *buffer)
{
    if (atapi_device < 0)
        return -1;
    return atapi_read_block(lba, buffer);
}

static int iso9660_mount_selected(void)
{
    uint32_t lba;
    int have_primary = 0;
    int have_joliet = 0;
    uint32_t primary_volume_sectors = 0;
    uint32_t primary_root_lba = 0;
    uint32_t primary_root_size = 0;
    uint32_t joliet_volume_sectors = 0;
    uint32_t joliet_root_lba = 0;
    uint32_t joliet_root_size = 0;
    iso_mounted = 0;
    iso_joliet = 0;
    atapi_device = -1;

    if (!iso_ata_image && iso9660_detect_device() != 0)
        return -1;

    for (lba = 16; lba < 80; ++lba) {
        uint8_t *descriptor;
        uint32_t volume_sectors;
        uint32_t root_lba;
        uint32_t root_size;

        if (iso_read_block(lba, iso_sector) != 0)
            return -1;

        descriptor = iso_sector;

        if (descriptor[1] != 'C' ||
            descriptor[2] != 'D' ||
            descriptor[3] != '0' ||
            descriptor[4] != '0' ||
            descriptor[5] != '1' ||
            descriptor[6] != 1)
            return -1;

        if (descriptor[0] == 255)
            break;

        if (descriptor[0] != 1 && descriptor[0] != 2)
            continue;

        volume_sectors = iso_le32(descriptor + 80);
        root_lba = iso_le32(descriptor + ISO_ROOT_RECORD + 2);
        root_size = iso_le32(descriptor + ISO_ROOT_RECORD + 10);

        if (volume_sectors == 0 || root_lba >= volume_sectors ||
            root_size == 0 ||
            (root_size - 1) / ISO_SECTOR_SIZE >= volume_sectors ||
            iso_le16(descriptor + 128) != ISO_SECTOR_SIZE)
            continue;

        if (descriptor[0] == 1) {
            primary_volume_sectors = volume_sectors;
            primary_root_lba = root_lba;
            primary_root_size = root_size;
            have_primary = 1;
        } else if (descriptor[88] == '%' && descriptor[89] == '/' &&
                   (descriptor[90] == '@' ||
                    descriptor[90] == 'C' ||
                    descriptor[90] == 'E')) {
            /*
             * Joliet escape sequences identify Supplementary Volume
             * Descriptors: %/@ (Level 1), %/C (Level 2), or %/E (Level 3).
             * Prefer the last recognized SVD if several are present.
             */
            joliet_volume_sectors = volume_sectors;
            joliet_root_lba = root_lba;
            joliet_root_size = root_size;
            have_joliet = 1;
        }
    }

    if (have_joliet) {
        iso_volume_sectors = joliet_volume_sectors;
        iso_root_lba = joliet_root_lba;
        iso_root_size = joliet_root_size;
        iso_joliet = 1;
    } else if (have_primary) {
        iso_volume_sectors = primary_volume_sectors;
        iso_root_lba = primary_root_lba;
        iso_root_size = primary_root_size;
        iso_joliet = 0;
    } else {
        return -1;
    }

    iso_mounted = 1;
    return 0;
}

int iso9660_mount(void)
{
    iso_ata_image = 0;
    iso_image_start_lba = 0;
    return iso9660_mount_selected();
}

int iso9660_mount_image(uint32_t image_start_lba)
{
    iso_image_start_lba = image_start_lba;
    iso_ata_image = 1;
    return iso9660_mount_selected();
}

int iso9660_is_mounted(void)
{
    return iso_mounted;
}

int iso9660_write_file(const char *path, const void *buffer, uint32_t size)
{
    uint32_t extent, file_size, copied = 0;
    int is_directory;

    if (!iso_mounted || !iso_ata_image || (!buffer && size))
        return -1;
    if (iso_resolve(path, &extent, &file_size, &is_directory) != 0 ||
        is_directory || size != file_size)
        return -1;

    while (copied < size) {
        uint32_t amount = size - copied;
        uint32_t block = extent + copied / ISO_SECTOR_SIZE;

        if (amount > ISO_SECTOR_SIZE)
            amount = ISO_SECTOR_SIZE;
        if (iso_read_block(block, iso_sector) != 0)
            return -1;

        for (uint32_t i = 0; i < amount; ++i)
            iso_sector[i] = ((const uint8_t *)buffer)[copied + i];

        if (block >= iso_volume_sectors ||
            block > 0xFFFFFFFFu / 4u ||
            iso_image_start_lba > 0xFFFFFFFFu - block * 4u - 3u)
            return -1;

        for (uint32_t i = 0; i < 4; ++i) {
            if (ata_write_sector(iso_image_start_lba + block * 4u + i,
                                 iso_sector + i * 512u) != 0)
                return -1;
        }
        copied += amount;
    }
    return (int)copied;
}

int iso9660_read_file(const char *path, void *buffer, uint32_t capacity)
{
    uint32_t extent;
    uint32_t size;
    uint32_t copied = 0;
    uint32_t sector_count;
    uint32_t i;
    int is_directory;

    if (!iso_mounted || path == 0 || buffer == 0)
        return -1;

    if (iso_resolve(path, &extent, &size, &is_directory) != 0 ||
        is_directory)
        return -1;

    if (size > 0x7FFFFFFF)
        return -1;

    if (capacity < size)
        return -22;

    sector_count = (size + ISO_SECTOR_SIZE - 1) / ISO_SECTOR_SIZE;

    for (i = 0; i < sector_count; ++i) {
        uint32_t amount = size - copied;

        if (amount > ISO_SECTOR_SIZE)
            amount = ISO_SECTOR_SIZE;

        if (iso_read_block(extent + i, iso_sector) != 0)
            return -1;

        {
            uint32_t j;
            uint8_t *out = (uint8_t *)buffer + copied;

            for (j = 0; j < amount; ++j)
                out[j] = iso_sector[j];
        }

        copied += amount;
    }

    return (int)size;
}

int iso9660_list_directory(const char *path, char *buffer, uint32_t capacity)
{
    uint32_t extent;
    uint32_t size;
    uint32_t offset = 0;
    uint32_t used = 0;
    int is_directory;

    if (!iso_mounted || path == 0 || buffer == 0 || capacity == 0)
        return -1;

    if (iso_resolve(path, &extent, &size, &is_directory) != 0 ||
        !is_directory)
        return -1;

    while (offset < size) {
        uint32_t sector_lba = extent + offset / ISO_SECTOR_SIZE;
        uint32_t sector_offset = offset % ISO_SECTOR_SIZE;
        uint8_t *record;
        uint8_t record_length;
        char name[256];
        int name_length;
        int directory;

        if (iso_read_block(sector_lba, iso_sector) != 0)
            return -1;

        record = iso_sector + sector_offset;
        record_length = record[0];

        if (record_length == 0) {
            offset += ISO_SECTOR_SIZE - sector_offset;
            continue;
        }

        if (record_length < 34 ||
            sector_offset + record_length > ISO_SECTOR_SIZE ||
            offset + record_length > size)
            return -1;

        name_length = iso_record_name(record, name, sizeof(name));
        directory = (record[25] & 0x02) != 0;

        if (name_length > 0) {
            uint32_t needed = (uint32_t)name_length + 1 +
                              (directory ? 1 : 0);

            if (needed > capacity - used)
                return -22;

            {
                int i;
                for (i = 0; i < name_length; ++i)
                    buffer[used++] = name[i];
            }

            if (directory)
                buffer[used++] = '/';

            buffer[used++] = '\n';
        }

        offset += record_length;
    }

    return (int)used;
}
