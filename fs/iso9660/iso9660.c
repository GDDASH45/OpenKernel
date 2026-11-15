#include <fs/iso9660/iso9660.h>
#include <kernel/ports.h>
#include <stdint.h>

#define ATAPI_DATA          0x1F0
#define ATAPI_ERROR         0x1F1
#define ATAPI_FEATURES      0x1F1
#define ATAPI_BYTE_COUNT_L  0x1F4
#define ATAPI_BYTE_COUNT_H  0x1F5
#define ATAPI_DRIVE         0x1F6
#define ATAPI_STATUS        0x1F7
#define ATAPI_COMMAND       0x1F7

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

static int atapi_device = -1;
static int iso_mounted;
static uint32_t iso_volume_sectors;
static uint32_t iso_root_lba;
static uint32_t iso_root_size;
static uint8_t iso_sector[ISO_SECTOR_SIZE];

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

static void atapi_delay(void)
{
    (void)port_byte_in(ATAPI_STATUS);
    (void)port_byte_in(ATAPI_STATUS);
    (void)port_byte_in(ATAPI_STATUS);
    (void)port_byte_in(ATAPI_STATUS);
}

static int atapi_wait(uint8_t required)
{
    uint32_t timeout = ATAPI_TIMEOUT;

    while (timeout-- != 0) {
        uint8_t status = port_byte_in(ATAPI_STATUS);

        if (status == 0x00 || status == 0xFF)
            return -1;

        if (status & ATAPI_STATUS_BUSY)
            continue;

        if (status & ATAPI_STATUS_ERROR) {
            (void)port_byte_in(ATAPI_ERROR);
            return -1;
        }

        if ((status & required) == required)
            return 0;
    }

    return -1;
}

static void atapi_select(int device)
{
    port_byte_out(ATAPI_DRIVE, (uint8_t)(0xA0 | (device ? 0x10 : 0)));
    atapi_delay();
}

/*
 * IDENTIFY PACKET DEVICE distinguishes ATAPI devices from ATA disks.
 * The identify payload is consumed so the device is left ready for commands.
 */
static int atapi_probe(int device)
{
    uint16_t identify[256];
    uint32_t i;

    atapi_select(device);

    if (port_byte_in(ATAPI_STATUS) == 0x00)
        return -1;

    port_byte_out(ATAPI_FEATURES, 0);
    port_byte_out(ATAPI_COMMAND, ATAPI_CMD_IDENTIFY);

    if (atapi_wait(ATAPI_STATUS_DRQ) != 0)
        return -1;

    atapi_insw(ATAPI_DATA, identify, 256);

    for (i = 0; i < 256; ++i)
        identify[i] = 0;

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

    port_byte_out(ATAPI_FEATURES, 0);
    port_byte_out(ATAPI_BYTE_COUNT_L, 0x00);
    port_byte_out(ATAPI_BYTE_COUNT_H, 0x08);
    port_byte_out(ATAPI_COMMAND, ATAPI_CMD_PACKET);

    if (atapi_wait(ATAPI_STATUS_DRQ) != 0)
        return -1;

    atapi_outsw(ATAPI_DATA, (const uint16_t *)packet, 6);

    timeout = ATAPI_TIMEOUT;

    while (timeout-- != 0) {
        uint16_t byte_count;
        uint32_t words;

        status = port_byte_in(ATAPI_STATUS);

        if (status == 0x00 || status == 0xFF)
            return -1;

        if (status & ATAPI_STATUS_BUSY)
            continue;

        if (status & ATAPI_STATUS_ERROR) {
            (void)port_byte_in(ATAPI_ERROR);
            return -1;
        }

        if (!(status & ATAPI_STATUS_DRQ))
            break;

        byte_count = (uint16_t)port_byte_in(ATAPI_BYTE_COUNT_L);
        byte_count |= (uint16_t)port_byte_in(ATAPI_BYTE_COUNT_H) << 8;

        if (byte_count == 0 || (byte_count & 1))
            return -1;

        words = byte_count / 2;

        if (transferred + byte_count > ISO_SECTOR_SIZE)
            return -1;

        atapi_insw(
            ATAPI_DATA,
            (uint16_t *)((uint8_t *)buffer + transferred),
            words
        );

        transferred += byte_count;
    }

    if (timeout == 0 || transferred != ISO_SECTOR_SIZE)
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

static int iso_name_length(const uint8_t *record)
{
    int length = record[32];

    if (length == 1 && (record[33] == 0 || record[33] == 1))
        return 0;

    while (length > 0 && record[33 + length - 1] == ';')
        --length;

    while (length > 0 && record[33 + length - 1] >= '0' &&
           record[33 + length - 1] <= '9')
        --length;

    if (length > 0 && record[33 + length - 1] == ';')
        --length;

    if (length > 0 && record[33 + length - 1] == '.')
        --length;

    return length;
}

static int iso_record_name(const uint8_t *record, char *name, uint32_t capacity)
{
    int length = iso_name_length(record);
    int i;

    if (length <= 0 || (uint32_t)length + 1 > capacity)
        return -1;

    for (i = 0; i < length; ++i) {
        uint8_t c = record[33 + i];

        if (c < 0x20 || c > 0x7E)
            return -1;

        name[i] = (char)c;
    }

    name[length] = 0;
    return length;
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
        char name[256];
        int name_length;

        if (atapi_read_block(sector_lba, iso_sector) != 0)
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
        char component[256];
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

int iso9660_mount(void)
{
    uint32_t lba;
    int device;

    iso_mounted = 0;
    atapi_device = -1;

    for (device = 0; device < 2; ++device) {
        if (atapi_probe(device) == 0) {
            atapi_device = device;
            break;
        }
    }

    if (atapi_device < 0)
        return -1;

    for (lba = 16; lba < 80; ++lba) {
        uint8_t *descriptor;

        if (atapi_read_block(lba, iso_sector) != 0)
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

        if (descriptor[0] != 1)
            continue;

        iso_volume_sectors = iso_le32(descriptor + 80);
        iso_root_lba = iso_le32(descriptor + ISO_ROOT_RECORD + 2);
        iso_root_size = iso_le32(descriptor + ISO_ROOT_RECORD + 10);

        if (iso_volume_sectors == 0 ||
            iso_root_lba >= iso_volume_sectors ||
            iso_root_size == 0 ||
            iso_root_size > iso_volume_sectors * ISO_SECTOR_SIZE)
            return -1;

        iso_mounted = 1;
        return 0;
    }

    return -1;
}

int iso9660_is_mounted(void)
{
    return iso_mounted;
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

        if (atapi_read_block(extent + i, iso_sector) != 0)
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

        if (atapi_read_block(sector_lba, iso_sector) != 0)
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
