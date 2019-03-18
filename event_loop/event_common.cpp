#include "zrtc/event_loop/event_common.h"

#include "zrtc/event_loop/libevent.h"

namespace evloop {

int EventAdd(struct event *ev, const struct timeval *timeout) {
	return event_add(ev, timeout);
}

int EventDel(struct event *ev) {
	return event_del(ev);
}
	
} // namespace evloop