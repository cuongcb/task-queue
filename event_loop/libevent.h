/* 
 * File:   libevent.h
 * Author: lap11894
 *
 * Created on January 8, 2019, 3:04 PM
 */

#ifdef ZRTC_WIN
#include <WinSock2.h>
#endif

#define H_LIBEVENT_VERSION_14

#ifdef H_LIBEVENT_VERSION_14
#include "zrtc/event_loop/third_party/libevent/event.h"
#include "zrtc/event_loop/third_party/libevent/evhttp.h"
#include "zrtc/event_loop/third_party/libevent/evutil.h"
#include "zrtc/event_loop/third_party/libevent/evdns.h"
#else
#include "zrtc/event_loop/third_party/libevent/event2/event.h"
#include "zrtc/event_loop/third_party/libevent/event2/event_struct.h"
#include "zrtc/event_loop/third_party/libevent/event2/buffer.h"
#include "zrtc/event_loop/third_party/libevent/event2/bufferevent.h"
#include "zrtc/event_loop/third_party/libevent/event2/http.h"
#include "zrtc/event_loop/third_party/libevent/event2/http_compat.h"
#include "zrtc/event_loop/third_party/libevent/event2/http_struct.h"
#include "zrtc/event_loop/third_party/libevent/event2/event_compat.h"
#include "zrtc/event_loop/third_party/libevent/event2/dns.h"
#include "zrtc/event_loop/third_party/libevent/event2/dns_compat.h"
#include "zrtc/event_loop/third_party/libevent/event2/dns_struct.h"
#include "zrtc/event_loop/third_party/libevent/event2/listener.h"
#ifdef _DEBUG
#if LIBEVENT_VERSION_NUMBER >= 0x02010500
#define  ev_arg ev_evcallback.evcb_arg
#endif // LIBEVENT_VERSION_NUMBER
#endif // _DEBUG
#endif // H_LIBEVENT_VERSION_14

#ifndef evtimer_new
#define evtimer_new(b, cb, arg)        event_new((b), -1, 0, (cb), (arg))
#endif

#ifdef H_LIBEVENT_VERSION_14
extern "C" {
    struct evdns_base;
    struct event* event_new(struct event_base* base, int fd, short events, void(*cb)(int, short, void*), void* arg);
    void event_free(struct event* ev);
    evhttp_connection* evhttp_connection_base_new(
        struct event_base* base, struct evdns_base* dnsbase,
        const char* address, unsigned short port);
}

// There is a bug of event timer for libevent1.4 on windows platform.
//   libevent1.4 use '#define evtimer_set(ev, cb, arg)  event_set(ev, -1, 0, cb, arg)' to assign a timer,
//   but '#define event_initialized(ev) ((ev)->ev_flags & EVLIST_INIT && (ev)->ev_fd != (int)INVALID_HANDLE_VALUE)'
//   So, if we use a event timer on windows, event_initialized(ev) will never return true.
#ifdef event_initialized
#undef event_initialized
#endif
#define event_initialized(ev) ((ev)->ev_flags & EVLIST_INIT)

#endif