#ifndef _KERNEL_BITOPS_H
#define _KERNEL_BITOPS_H

#include <stdint.h>

/*
 * Set the specified bit in a bitmap.
 *
 * bit:
 *     Zero-based bit index.
 *
 * bitmap:
 *     Pointer to an array of 32-bit bitmap words.
 */
void set_bit(uint32_t bit, volatile uint32_t *bitmap);

/*
 * Clear the specified bit in a bitmap.
 */
void clear_bit(uint32_t bit, volatile uint32_t *bitmap);

/*
 * Test whether a bit is set.
 *
 * Returns:
 *     1 if the bit is set.
 *     0 if the bit is clear.
 */
int test_bit(uint32_t bit, const volatile uint32_t *bitmap);

/*
 * Set a bit and return its previous state.
 */
int test_and_set_bit(uint32_t bit, volatile uint32_t *bitmap);

/*
 * Clear a bit and return its previous state.
 */
int test_and_clear_bit(uint32_t bit, volatile uint32_t *bitmap);

#endif