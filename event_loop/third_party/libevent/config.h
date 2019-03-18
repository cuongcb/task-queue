#if defined(__native_client_nonsfi__)
#include "zrtc/event_loop/third_party/libevent/nacl_nonsfi/config.h"
#elif defined(__APPLE__)
#include "zrtc/event_loop/third_party/libevent/mac/config.h"
#elif defined(__ANDROID__)
#include "zrtc/event_loop/third_party/libevent/android/config.h"
#elif defined(__linux__)
#include "zrtc/event_loop/third_party/libevent/linux/config.h"
#elif defined(__FreeBSD__)
#include "zrtc/event_loop/third_party/libevent/freebsd/config.h"
#elif defined(__sun)
#include "zrtc/event_loop/third_party/libevent/solaris/config.h"
#elif defined(_AIX)
#include "zrtc/event_loop/third_party/libevent/aix/config.h"
#else
#error generate event-config.h for your platform
#endif
