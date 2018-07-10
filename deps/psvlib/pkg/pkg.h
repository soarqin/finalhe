#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*pkg_output_func)(const char* msg, ...);
typedef void (*pkg_error_func)(const char* msg, ...);
typedef void (*pkg_output_progress_init_func)(uint64_t size);
typedef void (*pkg_output_progress_func)(uint64_t progress);

int pkg_dec(const char *pkgname, const char *zrif);
void pkg_disable_output();
void pkg_set_func(pkg_output_func out, pkg_error_func err,
                 pkg_output_progress_init_func proginit,
                 pkg_output_progress_func prog);

#ifdef __cplusplus
}
#endif
