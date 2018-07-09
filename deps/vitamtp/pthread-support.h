#ifndef _PTHREAD_SUPPORT_H__
#define _PTHREAD_SUPPORT_H__

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#define pthread_mutex_t CRITICAL_SECTION
#define pthread_mutex_lock EnterCriticalSection
#define pthread_mutex_unlock LeaveCriticalSection
#define pthread_mutex_init(m, o) InitializeCriticalSection(m)
#define pthread_mutex_destroy DeleteCriticalSection
#else
#include <pthread.h>
#endif

#endif
