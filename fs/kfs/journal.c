
#include <stdint.h>
#include <stddef.h>

#include <fs/kfs.h>
#include <fs/kfs_journal.h>

/*
 * KFS Journal
 * OpenKernel
 *
 * Write-ahead logging for filesystem metadata.
 *
 * On-disk layout:
 *
 *   start + 0 : journal header
 *   start + 1 : entry 0 descriptor
 *   start + 2 : entry 0 data
 *   start + 3 : entry 1 descriptor
 *   start + 4 : entry 1 data
 *   ...
 *
 * Each entry stores a complete filesystem block.
 *
 * Transaction protocol:
 *   1. Write staged entry descriptors and data.
 *   2. Persist the ACTIVE header with the entry count.
 *   3. Mark the transaction COMMITTED.
 *   4. Replay committed entries to their target blocks.
 *   5. Mark the journal CLEAN.
 *
 * This implementation supports one active journal instance.
 */

#define KFS_JOURNAL_ENTRY_BLOCKS  2u

static struct kfs_journal kfs_journal_state;

/*
 * FNV-1a checksum.
 *
 * This detects accidental corruption. It is not a
 * cryptographic integrity mechanism.
 */
static uint32_t
kfs_journal_checksum(const void *data, size_t size)
{
    const uint8_t *bytes = (const uint8_t *)data;
    uint32_t hash = 2166136261u;
    size_t i;

    for (i = 0; i < size; ++i) {
        hash ^= bytes[i];
        hash *= 16777619u;
    }

    return hash;
}

/*
 * Calculate the checksum of a journal header.
 * The checksum field is treated as zero.
 */
static uint32_t
kfs_journal_header_checksum(struct kfs_journal_header *header)
{
    uint32_t previous = header->checksum;
    uint32_t checksum;

    header->checksum = 0;
    checksum = kfs_journal_checksum(header, sizeof(*header));
    header->checksum = previous;

    return checksum;
}

/*
 * Read the journal header from disk.
 */
static int
kfs_journal_read_header(struct kfs_journal_header *header)
{
    uint8_t block[KFS_BLOCK_SIZE];
    uint8_t *destination = (uint8_t *)header;
    size_t i;

    if (kfs_journal_state.fs == NULL || header == NULL)
        return -1;

    if (kfs_read_block(
            kfs_journal_state.fs,
            kfs_journal_state.start_block,
            block) != 0)
        return -1;

    for (i = 0; i < sizeof(*header); ++i)
        destination[i] = block[i];

    return 0;
}

/*
 * Write the current journal header.
 */
static int
kfs_journal_write_header(void)
{
    uint8_t block[KFS_BLOCK_SIZE];
    struct kfs_journal_header *header;
    size_t i;

    if (kfs_journal_state.fs == NULL)
        return -1;

    for (i = 0; i < sizeof(block); ++i)
        block[i] = 0;

    header = (struct kfs_journal_header *)block;

    header->magic = KFS_JOURNAL_MAGIC;
    header->version = KFS_JOURNAL_VERSION;
    header->state = kfs_journal_state.state;
    header->entry_count = kfs_journal_state.entry_count;
    header->sequence = kfs_journal_state.sequence;
    header->checksum = 0;
    header->checksum = kfs_journal_header_checksum(header);

    return kfs_write_block(
        kfs_journal_state.fs,
        kfs_journal_state.start_block,
        block
    );
}

/*
 * Validate a header read from disk.
 */
static int
kfs_journal_validate_header(struct kfs_journal_header *header)
{
    if (header->magic != KFS_JOURNAL_MAGIC)
        return -1;

    if (header->version != KFS_JOURNAL_VERSION)
        return -1;

    if (header->state != KFS_JOURNAL_CLEAN &&
        header->state != KFS_JOURNAL_ACTIVE &&
        header->state != KFS_JOURNAL_COMMITTED)
        return -1;

    if (header->entry_count >
        (kfs_journal_state.block_count - 1u) /
        KFS_JOURNAL_ENTRY_BLOCKS)
        return -1;

    if (header->entry_count > KFS_JOURNAL_MAX_ENTRIES)
        return -1;

    if (header->checksum !=
        kfs_journal_header_checksum(header))
        return -1;

    return 0;
}

/*
 * Calculate the maximum number of journal entries.
 */
static uint32_t
kfs_journal_capacity(void)
{
    uint32_t capacity;

    capacity = (kfs_journal_state.block_count - 1u) /
               KFS_JOURNAL_ENTRY_BLOCKS;

    if (capacity > KFS_JOURNAL_MAX_ENTRIES)
        capacity = KFS_JOURNAL_MAX_ENTRIES;

    return capacity;
}

/*
 * Return the descriptor block for an entry.
 */
static uint32_t
kfs_journal_entry_descriptor_block(uint32_t index)
{
    return kfs_journal_state.start_block + 1u +
           index * KFS_JOURNAL_ENTRY_BLOCKS;
}

/*
 * Return the data block for an entry.
 */
static uint32_t
kfs_journal_entry_data_block(uint32_t index)
{
    return kfs_journal_entry_descriptor_block(index) + 1u;
}

/*
 * Initialize or load a journal region.
 *
 * A valid existing header is preserved so that interrupted
 * transactions can be recovered. An invalid or unformatted
 * journal is initialized as CLEAN.
 *
 * IMPORTANT: Format the journal region only when it is known
 * not to contain recoverable transactions.
 */
int
kfs_journal_init(struct kfs_fs *fs,
                 uint32_t start_block,
                 uint32_t block_count)
{
    struct kfs_journal_header header;
    size_t i;

    if (fs == NULL || !fs->mounted)
        return -1;

    if (kfs_journal_state.active)
        return -1;

    if (block_count < 3u ||
        start_block >= fs->block_count ||
        block_count > fs->block_count - start_block)
        return -1;

    kfs_journal_state.fs = fs;
    kfs_journal_state.start_block = start_block;
    kfs_journal_state.block_count = block_count;
    kfs_journal_state.sequence = 0;
    kfs_journal_state.entry_count = 0;
    kfs_journal_state.state = KFS_JOURNAL_CLEAN;
    kfs_journal_state.active = 1;

    if (kfs_journal_read_header(&header) != 0)
        goto fail;

    if (kfs_journal_validate_header(&header) != 0) {
        /*
         * Treat an invalid header as an unformatted journal.
         * Callers must not use this path on a journal that
         * may contain an interrupted transaction.
         */
        kfs_journal_state.state = KFS_JOURNAL_CLEAN;
        kfs_journal_state.entry_count = 0;
        kfs_journal_state.sequence = 0;

        if (kfs_journal_write_header() != 0)
            goto fail;

        return 0;
    }

    kfs_journal_state.state = header.state;
    kfs_journal_state.entry_count = header.entry_count;
    kfs_journal_state.sequence = header.sequence;

    for (i = 0; i < 1; ++i) {
        if (header.state == KFS_JOURNAL_COMMITTED) {
            if (kfs_journal_recover(fs) != 0)
                goto fail;
        } else if (header.state == KFS_JOURNAL_ACTIVE) {
            /*
             * ACTIVE means the transaction was not committed.
             * Its staged entries must not be replayed.
             */
            kfs_journal_state.state = KFS_JOURNAL_CLEAN;
            kfs_journal_state.entry_count = 0;

            if (kfs_journal_write_header() != 0)
                goto fail;
        }
    }

    return 0;

fail:
    kfs_journal_state.fs = NULL;
    kfs_journal_state.active = 0;
    return -1;
}

/*
 * Begin a new transaction.
 */
int
kfs_journal_begin(void)
{
    if (!kfs_journal_state.active)
        return -1;

    if (kfs_journal_state.state != KFS_JOURNAL_CLEAN)
        return -1;

    kfs_journal_state.state = KFS_JOURNAL_ACTIVE;
    kfs_journal_state.entry_count = 0;
    ++kfs_journal_state.sequence;

    if (kfs_journal_write_header() != 0) {
        kfs_journal_state.state = KFS_JOURNAL_CLEAN;
        return -1;
    }

    return 0;
}

/*
 * Stage a complete metadata block.
 */
int
kfs_journal_stage(uint32_t target_block, const void *data)
{
    uint8_t descriptor_block[KFS_BLOCK_SIZE];
    uint8_t data_block[KFS_BLOCK_SIZE];
    struct kfs_journal_entry_header *descriptor;
    uint32_t index;
    uint32_t descriptor_number;
    uint32_t data_number;
    const uint8_t *source = (const uint8_t *)data;
    size_t i;

    if (!kfs_journal_state.active ||
        kfs_journal_state.state != KFS_JOURNAL_ACTIVE ||
        data == NULL)
        return -1;

    if (target_block >= kfs_journal_state.fs->block_count)
        return -1;

    if (target_block >= kfs_journal_state.start_block &&
        target_block - kfs_journal_state.start_block <
            kfs_journal_state.block_count)
        return -1;

    if (kfs_journal_state.entry_count >= kfs_journal_capacity())
        return -1;

    index = kfs_journal_state.entry_count;

    descriptor_number = kfs_journal_entry_descriptor_block(index);
    data_number = kfs_journal_entry_data_block(index);

    for (i = 0; i < KFS_BLOCK_SIZE; ++i) {
        descriptor_block[i] = 0;
        data_block[i] = source[i];
    }

    descriptor =
        (struct kfs_journal_entry_header *)descriptor_block;

    descriptor->target_block = target_block;
    descriptor->checksum = kfs_journal_checksum(
        data_block,
        KFS_BLOCK_SIZE
    );

    /*
     * Write data first, then its descriptor.
     */
    if (kfs_write_block(
            kfs_journal_state.fs,
            data_number,
            data_block) != 0)
        return -1;

    if (kfs_write_block(
            kfs_journal_state.fs,
            descriptor_number,
            descriptor_block) != 0)
        return -1;

    ++kfs_journal_state.entry_count;

    /*
     * Persist the entry count so the journal header describes
     * all entries that have been staged so far.
     */
    if (kfs_journal_write_header() != 0) {
        --kfs_journal_state.entry_count;
        return -1;
    }

    return 0;
}

/*
 * Replay a committed transaction.
 */
int
kfs_journal_recover(struct kfs_fs *fs)
{
    uint8_t descriptor_block[KFS_BLOCK_SIZE];
    uint8_t data_block[KFS_BLOCK_SIZE];
    struct kfs_journal_header header;
    struct kfs_journal_entry_header *descriptor;
    uint32_t i;

    if (fs == NULL ||
        !kfs_journal_state.active ||
        kfs_journal_state.fs != fs)
        return -1;

    if (kfs_journal_read_header(&header) != 0)
        return -1;

    if (kfs_journal_validate_header(&header) != 0)
        return -1;

    kfs_journal_state.state = header.state;
    kfs_journal_state.entry_count = header.entry_count;
    kfs_journal_state.sequence = header.sequence;

    if (header.state == KFS_JOURNAL_CLEAN)
        return 0;

    if (header.state == KFS_JOURNAL_ACTIVE) {
        /*
         * An uncommitted transaction is discarded.
         */
        kfs_journal_state.state = KFS_JOURNAL_CLEAN;
        kfs_journal_state.entry_count = 0;

        return kfs_journal_write_header();
    }

    if (header.state != KFS_JOURNAL_COMMITTED)
        return -1;

    for (i = 0; i < header.entry_count; ++i) {
        uint32_t descriptor_number =
            kfs_journal_entry_descriptor_block(i);
        uint32_t data_number =
            kfs_journal_entry_data_block(i);
        uint32_t target_block;

        if (kfs_read_block(
                fs,
                descriptor_number,
                descriptor_block) != 0)
            return -1;

        if (kfs_read_block(
                fs,
                data_number,
                data_block) != 0)
            return -1;

        descriptor =
            (struct kfs_journal_entry_header *)descriptor_block;

        target_block = descriptor->target_block;

        if (target_block >= fs->block_count)
            return -1;

        if (target_block >= kfs_journal_state.start_block &&
            target_block - kfs_journal_state.start_block <
                kfs_journal_state.block_count)
            return -1;

        if (descriptor->checksum !=
            kfs_journal_checksum(data_block, KFS_BLOCK_SIZE))
            return -1;

        if (kfs_write_block(fs, target_block, data_block) != 0)
            return -1;
    }

    /*
     * All committed entries have been replayed.
     */
    kfs_journal_state.state = KFS_JOURNAL_CLEAN;
    kfs_journal_state.entry_count = 0;

    return kfs_journal_write_header();
}

/*
 * Commit the current transaction.
 */
int
kfs_journal_commit(void)
{
    if (!kfs_journal_state.active ||
        kfs_journal_state.state != KFS_JOURNAL_ACTIVE)
        return -1;

    /*
     * Once COMMITTED is durable, recovery must replay the
     * entire transaction if execution is interrupted.
     */
    kfs_journal_state.state = KFS_JOURNAL_COMMITTED;

    if (kfs_journal_write_header() != 0) {
        kfs_journal_state.state = KFS_JOURNAL_ACTIVE;
        return -1;
    }

    return kfs_journal_recover(kfs_journal_state.fs);
}

/*
 * Abort an uncommitted transaction.
 */
int
kfs_journal_abort(void)
{
    if (!kfs_journal_state.active ||
        kfs_journal_state.state != KFS_JOURNAL_ACTIVE)
        return -1;

    kfs_journal_state.state = KFS_JOURNAL_CLEAN;
    kfs_journal_state.entry_count = 0;

    return kfs_journal_write_header();
}

/*
 * Close the journal when no transaction is active.
 */
int
kfs_journal_close(void)
{
    if (!kfs_journal_state.active)
        return -1;

    if (kfs_journal_state.state != KFS_JOURNAL_CLEAN)
        return -1;

    kfs_journal_state.fs = NULL;
    kfs_journal_state.start_block = 0;
    kfs_journal_state.block_count = 0;
    kfs_journal_state.sequence = 0;
    kfs_journal_state.entry_count = 0;
    kfs_journal_state.active = 0;

    return 0;
}
