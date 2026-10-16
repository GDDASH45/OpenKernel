
#ifndef OPENKERNEL_FS_KFS_JOURNAL_H
#define OPENKERNEL_FS_KFS_JOURNAL_H

#include <stdint.h>
#include <stddef.h>

#include <fs/kfs.h>

/*
 * KFS Journal
 * OpenKernel
 *
 * Write-ahead logging interface for KFS metadata.
 */

/* Journal format */
#define KFS_JOURNAL_MAGIC          0x4B4A4E4Cu /* "KJNL" */
#define KFS_JOURNAL_VERSION        1u

/* Journal transaction states */
#define KFS_JOURNAL_CLEAN          0u
#define KFS_JOURNAL_ACTIVE         1u
#define KFS_JOURNAL_COMMITTED      2u

/* Initial transaction limits */
#define KFS_JOURNAL_MAX_ENTRIES    16u

/*
 * On-disk journal header.
 *
 * Occupies one complete filesystem block.
 * checksum is calculated with the checksum field set to zero.
 */
struct kfs_journal_header {
    uint32_t magic;
    uint32_t version;
    uint32_t state;
    uint32_t entry_count;
    uint32_t sequence;
    uint32_t checksum;

    uint8_t reserved[
        KFS_BLOCK_SIZE - 6u * sizeof(uint32_t)
    ];
};

/*
 * Journal entry descriptor.
 *
 * target_block identifies the destination filesystem block.
 * checksum covers the complete metadata block being journaled.
 *
 * The descriptor and data must occupy separate disk blocks
 * unless a future format explicitly defines another layout.
 */
struct kfs_journal_entry_header {
    uint32_t target_block;
    uint32_t checksum;
};

/*
 * Runtime journal state.
 *
 * This structure is not part of the on-disk format.
 */
struct kfs_journal {
    struct kfs_fs *fs;

    uint32_t start_block;
    uint32_t block_count;
    uint32_t sequence;
    uint32_t entry_count;
    uint32_t state;

    int active;
};

/* Initialize a journal region on a mounted filesystem. */
int kfs_journal_init(
    struct kfs_fs *fs,
    uint32_t start_block,
    uint32_t block_count
);

/* Begin a metadata transaction. */
int kfs_journal_begin(void);

/* Stage one complete metadata block for a transaction. */
int kfs_journal_stage(
    uint32_t target_block,
    const void *data
);

/* Commit staged metadata blocks. */
int kfs_journal_commit(void);

/* Abort an uncommitted transaction. */
int kfs_journal_abort(void);

/* Recover an interrupted transaction. */
int kfs_journal_recover(struct kfs_fs *fs);

/* Close an idle journal. */
int kfs_journal_close(void);

#endif /* OPENKERNEL_FS_KFS_JOURNAL_H */
