#include <kernel/bitops.h>

/*
 * OpenKernel bitmap operations
 *
 * This file provides basic operations for manipulating bitmaps.
 *
 * Bitmaps are arrays of 32-bit unsigned integers. Each integer
 * contains 32 independently addressable bits.
 *
 * These functions are non-atomic. They are intended for contexts
 * where concurrent modifications to the same bitmap cannot occur.
 *
 * TODO:
 *     Add atomic variants if OpenKernel requires SMP-safe bitmap
 *     operations in the future.
 */

/*
 * Calculate the bitmap word containing a requested bit.
 *
 * Dividing the bit index by 32 gives the array index.
 *
 * Example:
 *     bit 0  -> word 0
 *     bit 31 -> word 0
 *     bit 32 -> word 1
 */
#define BITMAP_WORD(bit) ((bit) / 32)

/*
 * Calculate the position of a bit inside its 32-bit word.
 *
 * The modulo operation keeps the position between 0 and 31.
 */
#define BITMAP_OFFSET(bit) ((bit) % 32)

/*
 * Generate a mask containing only the requested bit.
 *
 * Shifting an unsigned 1 by the bit offset creates a mask
 * that can be used with bitwise operations.
 */
#define BITMAP_MASK(bit) (1U << BITMAP_OFFSET(bit))

/*
 * Set a bit.
 *
 * The OR operation sets the requested bit without changing
 * any other bits in the bitmap word.
 */
void set_bit(uint32_t bit, volatile uint32_t *bitmap)
{
    /* Reject a NULL bitmap pointer. */
    if (bitmap == 0)
        return;

    /* Locate the word containing the requested bit. */
    uint32_t word = BITMAP_WORD(bit);

    /* Generate the mask for the requested bit. */
    uint32_t mask = BITMAP_MASK(bit);

    /* Set the bit while preserving all other bits. */
    bitmap[word] |= mask;
}

/*
 * Clear a bit.
 *
 * The inverted mask contains zero at the requested position
 * and ones everywhere else.
 *
 * ANDing with this mask clears only the requested bit.
 */
void clear_bit(uint32_t bit, volatile uint32_t *bitmap)
{
    /* Reject a NULL bitmap pointer. */
    if (bitmap == 0)
        return;

    /* Locate the bitmap word. */
    uint32_t word = BITMAP_WORD(bit);

    /* Generate the mask for the requested bit. */
    uint32_t mask = BITMAP_MASK(bit);

    /* Clear the requested bit. */
    bitmap[word] &= ~mask;
}

/*
 * Test a bit.
 *
 * This function does not modify the bitmap.
 *
 * Returns 1 when the requested bit is set, otherwise 0.
 */
int test_bit(uint32_t bit, const volatile uint32_t *bitmap)
{
    /* A NULL bitmap cannot contain a valid bit. */
    if (bitmap == 0)
        return 0;

    /* Locate the bitmap word. */
    uint32_t word = BITMAP_WORD(bit);

    /* Generate the requested bit mask. */
    uint32_t mask = BITMAP_MASK(bit);

    /* Check whether the requested bit is set. */
    return (bitmap[word] & mask) != 0;
}

/*
 * Test and set a bit.
 *
 * This function reads the previous state of the bit,
 * sets the bit, and returns its previous state.
 *
 * This operation is NOT atomic.
 */
int test_and_set_bit(uint32_t bit, volatile uint32_t *bitmap)
{
    /* Reject a NULL bitmap pointer. */
    if (bitmap == 0)
        return 0;

    /* Read the current state before modifying the bitmap. */
    int previous = test_bit(bit, bitmap);

    /* Set the requested bit. */
    set_bit(bit, bitmap);

    /* Return the state that existed before the operation. */
    return previous;
}

/*
 * Test and clear a bit.
 *
 * This function reads the previous state of the bit,
 * clears the bit, and returns its previous state.
 *
 * This operation is NOT atomic.
 */
int test_and_clear_bit(uint32_t bit, volatile uint32_t *bitmap)
{
    /* Reject a NULL bitmap pointer. */
    if (bitmap == 0)
        return 0;

    /* Read the current state before modifying the bitmap. */
    int previous = test_bit(bit, bitmap);

    /* Clear the requested bit. */
    clear_bit(bit, bitmap);

    /* Return the state that existed before the operation. */
    return previous;
}