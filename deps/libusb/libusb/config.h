#ifndef _CONFIG_H__
#define _CONFIG_H__

#ifdef _MSC_VER

#if (_MSC_VER >= 1900)
#define _TIMESPEC_DEFINED 1
#endif

#pragma warning(disable:4200)
#pragma warning(disable:4324)
#pragma warning(disable:6258)
#pragma warning(disable:4996)

#if defined(_PREFAST_)
#pragma warning(disable:28719)
#pragma warning(disable:28125)
#endif

#endif

#ifdef __GNUC__
#define DEFAULT_VISIBILITY __attribute__((visibility("default")))
#else
#define DEFAULT_VISIBILITY
#endif

#define ENABLE_LOGGING 1

#ifdef _WIN32
#define POLL_NFDS_TYPE unsigned int
#if defined(_WIN32_WCE)
#define OS_WINCE 1
#define HAVE_MISSING_H
#else
#define OS_WINDOWS 1
#define HAVE_SYS_TYPES_H 1
#endif
#else
#define POLL_NFDS_TYPE nfds_t
#endif

#endif //_CONFIG_H__
