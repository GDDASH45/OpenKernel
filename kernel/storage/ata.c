#include <kernel/ata.h>
#include <kernel/ports.h>

#define ATA_DATA 0x1F0
#define ATA_SECTOR_COUNT 0x1F2
#define ATA_LBA_LOW 0x1F3
#define ATA_LBA_MID 0x1F4
#define ATA_LBA_HIGH 0x1F5
#define ATA_DRIVE 0x1F6
#define ATA_STATUS 0x1F7
#define ATA_COMMAND 0x1F7

#define ATA_STATUS_ERROR 0x01
#define ATA_STATUS_DRQ 0x08
#define ATA_STATUS_BUSY 0x80

static void ata_insw(uint16_t port, uint16_t *buffer, uint32_t count) {
    __asm__ volatile ("rep insw" : "+D"(buffer), "+c"(count) : "d"(port) : "memory");
}

static void ata_outsw(uint16_t port, const uint16_t *buffer, uint32_t count) {
    __asm__ volatile ("rep outsw" : "+S"(buffer), "+c"(count) : "d"(port));
}

static int ata_wait(uint8_t required) {
    uint32_t timeout = 1000000;

    while (timeout-- != 0) {
        uint8_t status = port_byte_in(ATA_STATUS);
        if ((status & ATA_STATUS_ERROR) != 0) {
            return -1;
        }
        if ((status & (ATA_STATUS_BUSY | required)) == required) {
            return 0;
        }
    }
    return -1;
}

static void ata_select(uint32_t lba) {
    port_byte_out(ATA_DRIVE, (uint8_t)(0xE0 | ((lba >> 24) & 0x0F)));
    port_byte_in(ATA_STATUS);
    port_byte_in(ATA_STATUS);
    port_byte_in(ATA_STATUS);
    port_byte_in(ATA_STATUS);
}

static void ata_set_lba(uint32_t lba) {
    port_byte_out(ATA_SECTOR_COUNT, 1);
    port_byte_out(ATA_LBA_LOW, (uint8_t)lba);
    port_byte_out(ATA_LBA_MID, (uint8_t)(lba >> 8));
    port_byte_out(ATA_LBA_HIGH, (uint8_t)(lba >> 16));
}

int ata_read_sector(uint32_t lba, void *buffer) {
    if (buffer == 0 || lba >= 0x10000000) {
        return -1;
    }
    ata_select(lba);
    ata_set_lba(lba);
    port_byte_out(ATA_COMMAND, 0x20);
    if (ata_wait(ATA_STATUS_DRQ) != 0) {
        return -1;
    }
    ata_insw(ATA_DATA, (uint16_t *)buffer, 256);
    return 0;
}

int ata_write_sector(uint32_t lba, const void *buffer) {
    if (buffer == 0 || lba >= 0x10000000) {
        return -1;
    }
    ata_select(lba);
    ata_set_lba(lba);
    port_byte_out(ATA_COMMAND, 0x30);
    if (ata_wait(ATA_STATUS_DRQ) != 0) {
        return -1;
    }
    ata_outsw(ATA_DATA, (const uint16_t *)buffer, 256);
    port_byte_out(ATA_COMMAND, 0xE7);
    return ata_wait(0);
}