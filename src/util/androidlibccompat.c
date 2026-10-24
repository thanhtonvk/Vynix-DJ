// Fallbacks for libc functions that are referenced by the prebuilt
// dependencies (Qt, abseil, zix) and by Mixxx itself, but are only available
// on newer Android versions. Those libraries are built against API level 35,
// so without these the app fails to start on older devices with
// 'cannot locate symbol "copy_file_range"' and similar errors.
//
// This library is linked with -z global and loaded from Java before any Qt
// library, so the dynamic linker resolves these symbols here when libc does
// not provide them. If libc has the real implementation, it is used instead.

#include <dlfcn.h>
#include <errno.h>
#include <pthread.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/system_properties.h>
#include <sys/types.h>
#include <time.h>
#include <unwind.h>

#define EXPORT __attribute__((visibility("default")))

static void* libcSymbol(const char* name) {
    static void* s_libc = NULL;
    void* libc = __atomic_load_n(&s_libc, __ATOMIC_ACQUIRE);
    if (!libc) {
        libc = dlopen("libc.so", RTLD_NOW | RTLD_NOLOAD);
        if (!libc) {
            return NULL;
        }
        __atomic_store_n(&s_libc, libc, __ATOMIC_RELEASE);
    }
    return dlsym(libc, name);
}

// Resolves the libc implementation once. Racing threads resolve the same
// pointer, so no locking is required.
#define LIBC_FUNCTION(type, name)                                        \
    static type name##Real(void) {                                      \
        static type s_function = NULL;                                  \
        static int s_resolved = 0;                                      \
        if (!__atomic_load_n(&s_resolved, __ATOMIC_ACQUIRE)) {          \
            __atomic_store_n(&s_function, (type)libcSymbol(#name),      \
                    __ATOMIC_RELAXED);                                  \
            __atomic_store_n(&s_resolved, 1, __ATOMIC_RELEASE);         \
        }                                                               \
        return __atomic_load_n(&s_function, __ATOMIC_RELAXED);          \
    }

// API level 29
typedef int (*GetDeviceApiLevelFunction)(void);
LIBC_FUNCTION(GetDeviceApiLevelFunction, android_get_device_api_level)

EXPORT int android_get_device_api_level(void) {
    GetDeviceApiLevelFunction real = android_get_device_api_levelReal();
    if (real) {
        return real();
    }
    char value[PROP_VALUE_MAX] = {0};
    if (__system_property_get("ro.build.version.sdk", value) < 1) {
        return -1;
    }
    return atoi(value);
}

// API level 30
typedef int (*CondClockwaitFunction)(
        pthread_cond_t*, pthread_mutex_t*, clockid_t, const struct timespec*);
LIBC_FUNCTION(CondClockwaitFunction, pthread_cond_clockwait)

EXPORT int pthread_cond_clockwait(pthread_cond_t* cond,
        pthread_mutex_t* mutex,
        clockid_t clock,
        const struct timespec* abstime) {
    CondClockwaitFunction real = pthread_cond_clockwaitReal();
    if (real) {
        return real(cond, mutex, clock, abstime);
    }
    switch (clock) {
    case CLOCK_MONOTONIC:
        return pthread_cond_timedwait_monotonic_np(cond, mutex, abstime);
    case CLOCK_REALTIME:
        return pthread_cond_timedwait(cond, mutex, abstime);
    default:
        return EINVAL;
    }
}

// API level 33
typedef int (*BacktraceFunction)(void**, int);
LIBC_FUNCTION(BacktraceFunction, backtrace)

struct BacktraceState {
    void** buffer;
    int size;
    int count;
};

static _Unwind_Reason_Code backtraceCallback(struct _Unwind_Context* context, void* arg) {
    struct BacktraceState* state = (struct BacktraceState*)arg;
    if (state->count >= state->size) {
        return _URC_END_OF_STACK;
    }
    const uintptr_t ip = _Unwind_GetIP(context);
    if (ip) {
        state->buffer[state->count++] = (void*)ip;
    }
    return _URC_NO_REASON;
}

EXPORT int backtrace(void** buffer, int size) {
    BacktraceFunction real = backtraceReal();
    if (real) {
        return real(buffer, size);
    }
    if (!buffer || size <= 0) {
        return 0;
    }
    struct BacktraceState state = {buffer, size, 0};
    _Unwind_Backtrace(backtraceCallback, &state);
    return state.count;
}

// API level 34
typedef ssize_t (*CopyFileRangeFunction)(
        int, off64_t*, int, off64_t*, size_t, unsigned int);
LIBC_FUNCTION(CopyFileRangeFunction, copy_file_range)

EXPORT ssize_t copy_file_range(int fdIn,
        off64_t* offIn,
        int fdOut,
        off64_t* offOut,
        size_t length,
        unsigned int flags) {
    CopyFileRangeFunction real = copy_file_rangeReal();
    if (real) {
        return real(fdIn, offIn, fdOut, offOut, length, flags);
    }
    // Calling the syscall directly is not an option, older seccomp policies
    // kill the app for it. Callers fall back to read() and write().
    errno = ENOSYS;
    return -1;
}
