#ifndef BINFMT_OKX_H
#define BINFMT_OKX_H

#include <stdint.h>

int binfmt_okx_load(const void *file_data, uint32_t file_size,
                    void (**entry_out)(void));
int binfmt_oso_load(const void *file_data, uint32_t file_size,
                    void (**entry_out)(void));

#endif