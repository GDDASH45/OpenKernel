#ifndef BINFMT_PMX_H
#define BINFMT_PMX_H

#include <stdint.h>

int binfmt_pmx_load(const void *file_data, uint32_t file_size, void (**entry_out)(void));

#endif