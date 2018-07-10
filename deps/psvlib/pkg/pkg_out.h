#pragma once

#include <stdint.h>

void out_add_folder(const char* path);
uint64_t out_begin_file(const char* name);
void out_end_file(void);
void out_write(const void* buffer, uint32_t size);

// hacky solution to be able to write cso header after the data is written
void out_write_at(uint64_t offset, const void* buffer, uint32_t size);
void out_set_offset(uint64_t offset);
