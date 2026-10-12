#ifndef BINFMT_OKX2_H
#define BINFMT_OKX2_H

#include <stdint.h>

int binfmt_okx2_load(const void *file_data, uint32_t file_size,
                    void (**entry_out)(void));
int binfmt_oso2_load(const void *file_data, uint32_t file_size,
                    void (**entry_out)(void));

#endif