#include "pkg_out.h"
#include "pkg_sys.h"

static sys_file out_file;
static uint64_t out_file_offset;

void out_add_folder(const char* path)
{
    sys_mkdir(path);
}

uint64_t out_begin_file(const char* name)
{
    out_file = sys_create(name);
    out_file_offset = 0;
    return 0;
}

void out_end_file(void)
{
    sys_close(out_file);
}

void out_write(const void* buffer, uint32_t size)
{
    sys_write(out_file, out_file_offset, buffer, size);
    out_file_offset += size;
}

void out_write_at(uint64_t offset, const void* buffer, uint32_t size)
{
    sys_write(out_file, offset, buffer, size);
}

void out_set_offset(uint64_t offset)
{
    out_file_offset = offset;
}
