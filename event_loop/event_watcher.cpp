#include "zrtc/event_loop/event_watcher.h"

#include "zrtc/event_loop/libevent.h"

#include "zrtc/event_loop/event_loop.h"
#include "zrtc/event_loop/event_common.h"
#include "zrtc/event_loop/event_sockets.h"

namespace evloop {

EventWatcher::EventWatcher(struct ::event_base* evbase, const Handler& handler)
	: evbase_(evbase)
	, attached_(false)
	, handler_(handler) {
	event_ = new event();
	memset(event_, 0, sizeof(struct event));
}

EventWatcher::EventWatcher(struct ::event_base* evbase, Handler&& handler)
	: evbase_(evbase)
	, attached_(false)
	, handler_(std::move(handler)) {
	event_ = new event();
	memset(event_, 0, sizeof(struct event));
}

EventWatcher::~EventWatcher() {
	FreeEvent();
	Close();
}

bool EventWatcher::Init() {
	if (!DoInit()) {
		goto failed;
	}
	
	::event_base_set(evbase_, event_);
	return true;
	
failed:
	Close();
	return false;
}

void EventWatcher::Close() {
	DoClose();
}

bool EventWatcher::Watch(int timeout_ms) {
	struct timeval tv;
	struct timeval *timeoutval = nullptr;
	
	if (timeout_ms > 0) {
		tv.tv_sec = timeout_ms / 1000;
		tv.tv_usec = 0;
		timeoutval = &tv;
	}
	
	if (attached_) {
		// Prevent calling EventWatcher::Watch many times
		if (EventDel(event_) != 0) {
			LOG_T_F(LS_ERROR) << "event_del failed. fd=" << this->event_->ev_fd << " event_=" << event_;
		}
		
		attached_ = false;
	}
	
	assert(!attached_);
	
	if (EventAdd(event_, timeoutval) != 0) {
		LOG_T_F(LS_ERROR) << "event_add failed. fd=" << this->event_->ev_fd << " event_=" << event_;
		return false;
	}
	
	attached_ = true;
	
	return true;
}

void EventWatcher::FreeEvent() {
	if (event_) {
		if (attached_) {
			EventDel(event_);
			attached_ = false;
		}
		
		delete event_;
		event_ = nullptr;
	}
}

void EventWatcher::Cancel() {
	FreeEvent();
	
	if (cancel_callback_) {
		cancel_callback_();
	}
}

void EventWatcher::SetCancelCallback(const Handler& cb) {
	cancel_callback_ = cb;
}

////////////////////////////////////////////////////////////////////////////////
/////////////////////////////// PipeEventWatcher ///////////////////////////////
////////////////////////////////////////////////////////////////////////////////

PipeEventWatcher::PipeEventWatcher(EventLoop* loop, const Handler& handler)
	: EventWatcher(loop->event_base(), handler) {
	memset(pipe_, 0, sizeof(pipe_[0] * 2));
}

PipeEventWatcher::PipeEventWatcher(EventLoop* loop, Handler&& handler)
	: EventWatcher(loop->event_base(), std::move(handler)) {
	memset(pipe_, 0, sizeof(pipe_[0] * 2));
}

PipeEventWatcher::~PipeEventWatcher() {
	Close();
}

bool PipeEventWatcher::DoInit() {
	assert(pipe_[0] == 0);
	
	if (evutil_socketpair(AF_UNIX, SOCK_STREAM, 0, pipe_) < 0) {
		int err = errno;
		LOG_T_F(LS_ERROR) << "create socketpair ERROR errno=" << err << " " << strerror(err);
		goto failed;
	}
	
	if (evutil_make_socket_nonblocking(pipe_[0] < 0 ||
		evutil_make_socket_nonblocking(pipe_[1] < 0))) {
		goto failed;
	}
	
	event_set(event_, pipe_[1], EV_READ | EV_PERSIST,
			&PipeEventWatcher::HandlerFn, this);
	
	return true;
	
failed:
	Close();
	return false;
}

void PipeEventWatcher::DoClose() {
	if (pipe_[0] > 0) {
		EVUTIL_CLOSESOCKET(pipe_[0]);
		EVUTIL_CLOSESOCKET(pipe_[1]);
		memset(pipe_, 0, sizeof(pipe_[0] * 2));
	}
}

void PipeEventWatcher::HandlerFn(int fd, short which, void* v) {
	PipeEventWatcher *e = (PipeEventWatcher *)v;
	char buf[128];
	int n = 0;
	
	if ((n = ::recv(e->pipe_[1], buf, sizeof(buf), 0)) > 0) {
		e->handler_();
	}
}

bool PipeEventWatcher::AsyncWait() {
	return Watch(0);
}

void PipeEventWatcher::Notify() {
	char buf[1] = {};
	if (send(pipe_[0], buf, sizeof(buf), 0) < 0) {
		return;
	}
}

////////////////////////////////////////////////////////////////////////////////
/////////////////////////////// TimerEventWatcher //////////////////////////////
////////////////////////////////////////////////////////////////////////////////

TimerEventWatcher::TimerEventWatcher(EventLoop* loop,
									const Handler& handler,
									int timeout)
	: EventWatcher(loop->event_base(), handler)
	, timeout_ms_(timeout) {

}

TimerEventWatcher::TimerEventWatcher(EventLoop* loop,
									Handler&& handler,
									int timeout)
	: EventWatcher(loop->event_base(), std::move(handler))
	, timeout_ms_(timeout) {
	
}

TimerEventWatcher::~TimerEventWatcher() {
	
}

bool TimerEventWatcher::DoInit() {
	event_set(event_, -1, 0, &TimerEventWatcher::HandlerFn, this);
	return true;
}

void TimerEventWatcher::HandlerFn(int fd, short which, void* v) {
	TimerEventWatcher *t = (TimerEventWatcher *)v;
	t->handler_();
}

bool TimerEventWatcher::AsyncWait() {
	Watch(timeout_ms_);
}

} // namespace evloop

