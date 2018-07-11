#pragma once

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

typedef HANDLE pthread_t;

typedef struct {
    void *(*func)(void *);
    void *arg;
} _ThreadParam;

static DWORD WINAPI _ThreadProc(void* param) {
    _ThreadParam *p = (_ThreadParam*)param;
    DWORD result = (DWORD)(uintptr_t)p->func(p->arg);
    free(p);
    return result;
}

typedef struct pthread_attr_t pthread_attr_t;

static inline int pthread_create(pthread_t *th, const pthread_attr_t *attr, void *(*func)(void *), void *arg) {
    _ThreadParam *param = (_ThreadParam*)malloc(sizeof(_ThreadParam));
    param->func = func;
    param->arg = arg;
    *th = CreateThread(NULL, 0, _ThreadProc, param, 0, NULL);
    if (*th == NULL) return GetLastError();
    return 0;
}

static inline int pthread_join(pthread_t t, void **res) {
    DWORD code;
    if (WaitForSingleObject(t, INFINITE) == WAIT_FAILED) return -1;
    GetExitCodeThread(t, &code);
    CloseHandle(t);
    if (res != NULL) *res = (void*)(uintptr_t)code;
    return 0;
}
