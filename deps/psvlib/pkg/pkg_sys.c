#include "pkg_sys.h"
#include "pkg_utils.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>

#if defined(_WIN32)

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

static HANDLE gStdout;
static int gStdoutRedirected;
static UINT gOldCP;

void sys_output_init(void)
{
    gOldCP = GetConsoleOutputCP();
    SetConsoleOutputCP(CP_UTF8);
    gStdout = GetStdHandle(STD_OUTPUT_HANDLE);

    DWORD mode;
    gStdoutRedirected = !GetConsoleMode(gStdout, &mode);
}

void sys_output_done(void)
{
    SetConsoleOutputCP(gOldCP);
}

void sys_output(const char* msg, ...)
{
    char buffer[1024];

    va_list arg;
    va_start(arg, msg);
    vsnprintf(buffer, sizeof(buffer), msg, arg);
    va_end(arg);

    if (!gStdoutRedirected)
    {
        WCHAR wbuffer[sizeof(buffer)];
        int wcount = MultiByteToWideChar(CP_UTF8, 0, buffer, -1, wbuffer, sizeof(buffer));

        DWORD written;
        WriteConsoleW(GetStdHandle(STD_OUTPUT_HANDLE), wbuffer, wcount - 1, &written, NULL);
        return;
    }
    fputs(buffer, stdout);
}

void sys_error(const char* msg, ...)
{
    char buffer[1024];

    va_list arg;
    va_start(arg, msg);
    vsnprintf(buffer, sizeof(buffer), msg, arg);
    va_end(arg);

    DWORD mode;
    if (GetConsoleMode(GetStdHandle(STD_ERROR_HANDLE), &mode))
    {
        WCHAR wbuffer[sizeof(buffer)];
        int wcount = MultiByteToWideChar(CP_UTF8, 0, buffer, -1, wbuffer, sizeof(buffer));

        DWORD written;
        WriteConsoleW(GetStdHandle(STD_ERROR_HANDLE), wbuffer, wcount - 1, &written, NULL);
    }
    else
    {
        fputs(buffer, stderr);
    }

    SetConsoleOutputCP(gOldCP);
}

static void sys_mkdir_real(const char* path)
{
    WCHAR wpath[MAX_PATH];
    MultiByteToWideChar(CP_UTF8, 0, path, -1, wpath, MAX_PATH);

    if (CreateDirectoryW(wpath, NULL) == 0)
    {
        if (GetLastError() != ERROR_ALREADY_EXISTS)
        {
            sys_error("ERROR: cannot create '%s' folder\n", path);
        }
    }
}

sys_file sys_open(const char* fname, uint64_t* size)
{
    WCHAR path[MAX_PATH];
    MultiByteToWideChar(CP_UTF8, 0, fname, -1, path, MAX_PATH);

    HANDLE handle = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    if (handle == INVALID_HANDLE_VALUE)
    {
        sys_error("ERROR: cannot open '%s' file\n", fname);
    }

    LARGE_INTEGER sz;
    if (!GetFileSizeEx(handle, &sz))
    {
        sys_error("ERROR: cannot get size of '%s' file\n", fname);
    }
    *size = sz.QuadPart;

    return handle;
}

sys_file sys_create(const char* fname)
{
    WCHAR path[MAX_PATH];
    MultiByteToWideChar(CP_UTF8, 0, fname, -1, path, MAX_PATH);

    HANDLE handle = CreateFileW(path, GENERIC_READ | GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    if (handle == INVALID_HANDLE_VALUE)
    {
        sys_error("ERROR: cannot create '%s' file\n", fname);
    }

    return handle;
}

void sys_close(sys_file file)
{
    if (!CloseHandle(file))
    {
        sys_error("ERROR: failed to close file\n");
    }
}

void sys_read(sys_file file, uint64_t offset, void* buffer, uint32_t size)
{
    DWORD read;
    OVERLAPPED ov;
    ov.hEvent = NULL;
    ov.Offset = (uint32_t)offset;
    ov.OffsetHigh = (uint32_t)(offset >> 32);
    if (!ReadFile(file, buffer, size, &read, &ov) || read != size)
    {
        sys_error("ERROR: failed to read %u bytes from file\n", size);
    }
}

void sys_write(sys_file file, uint64_t offset, const void* buffer, uint32_t size)
{
    DWORD written;
    OVERLAPPED ov;
    ov.hEvent = NULL;
    ov.Offset = (uint32_t)offset;
    ov.OffsetHigh = (uint32_t)(offset >> 32);
    if (!WriteFile(file, buffer, size, &written, &ov) || written != size)
    {
        sys_error("ERROR: failed to write %u bytes to file\n", size);
    }
}

#else

#define _FILE_OFFSET_BITS 64
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <sys/stat.h>

static int gStdoutRedirected;

void sys_output_init(void)
{
    gStdoutRedirected = !isatty(STDOUT_FILENO);
}

void sys_output_done(void)
{
}

void sys_output(const char* msg, ...)
{
    va_list arg;
    va_start(arg, msg);
    vfprintf(stdout, msg, arg);
    va_end(arg);
}

void sys_error(const char* msg, ...)
{
    va_list arg;
    va_start(arg, msg);
    vfprintf(stderr, msg, arg);
    va_end(arg);

    exit(EXIT_FAILURE);
}

static void sys_mkdir_real(const char* path)
{
    if (mkdir(path, S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH) < 0)
    {
        if (errno != EEXIST)
        {
            sys_error("ERROR: cannot create '%s' folder\n", path);
        }
    }
}

sys_file sys_open(const char* fname, uint64_t* size)
{
    int fd = open(fname, O_RDONLY);
    if (fd < 0)
    {
        sys_error("ERROR: cannot open '%s' file\n", fname);
    }

    struct stat st;
    if (fstat(fd, &st) != 0)
    {
        sys_error("ERROR: cannot get size of '%s' file\n", fname);
    }
    *size = st.st_size;

    return (void*)(intptr_t)fd;
}

sys_file sys_create(const char* fname)
{
    int fd = open(fname, O_RDWR | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
    if (fd < 0)
    {
        sys_error("ERROR: cannot create '%s' file\n", fname);
    }

    return (void*)(intptr_t)fd;
}

void sys_close(sys_file file)
{
    if (close((int)(intptr_t)file) != 0)
    {
        sys_error("ERROR: failed to close file\n");
    }
}

void sys_read(sys_file file, uint64_t offset, void* buffer, uint32_t size)
{
    ssize_t read = pread((int)(intptr_t)file, buffer, size, offset);
    if (read < 0 || read != (ssize_t)size)
    {
        sys_error("ERROR: failed to read %u bytes from file\n", size);
    }
}

void sys_write(sys_file file, uint64_t offset, const void* buffer, uint32_t size)
{
    ssize_t wrote = pwrite((int)(intptr_t)file, buffer, size, offset);
    if (wrote < 0 || wrote != (ssize_t)size)
    {
        sys_error("ERROR: failed to read %u bytes from file\n", size);
    }
}

#endif

void sys_mkdir(const char* path)
{
    char* last = strrchr(path, '/');
    if (last)
    {
        *last = 0;
        sys_mkdir(path);
        *last = '/';
    }
    sys_mkdir_real(path);
}

void* sys_realloc(void* ptr, size_t size)
{
    void* result = NULL;
    if (!ptr && size)
    {
        result = malloc(size);
    }
    else if (ptr && !size)
    {
        free(ptr);
        return NULL;
    }
    else if (ptr && size)
    {
        result = realloc(ptr, size);
    }
    else
    {
        sys_error("ERROR: internal error, wrong sys_realloc usage\n");
    }

    if (!result)
    {
        sys_error("ERROR: out of memory\n");
    }

    return result;
}

void sys_vstrncat(char* dst, size_t n, const char* format, ...)
{
    char temp[1024];

    va_list args;
    va_start(args, format);
    vsnprintf(temp, sizeof(temp), format, args);
    va_end(args);

    strncat(dst, temp, n - strlen(dst) - 1);
}

static uint64_t out_size;
static uint32_t out_next;

void sys_output_progress_init(uint64_t size)
{
    out_size = size;
    out_next = 0;
}

void sys_output_progress(uint64_t progress)
{
    if (gStdoutRedirected)
    {
        return;
    }

    uint32_t now = (uint32_t)(progress * 100 / out_size);
    if (now >= out_next)
    {
        sys_output("[*] unpacking... %u%%\r", now);
        out_next = now + 1;
    }
}
