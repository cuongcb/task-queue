#include "zrtc/event_loop/fd_channel.h"

#include "zrtc/event_loop/libevent.h"
#include "zrtc/event_loop/event_loop.h"
#include "zrtc/event_loop/event_common.h"
#include "zrtc/event_loop/event_sockets.h"

namespace evloop {

FdChannel::FdChannel(evloop::EventLoop* loop,
		int fd,
		bool r,
		bool w)
	: loop_(loop)
	, attached_(false)
	, event_(nullptr)
	, fd_(fd) {
	flags_ = (r ? kReadable : 0) | (w ? kWritable : 0);
	event_ = new event();
	memset(event_, 0, sizeof(struct event));
}

FdChannel::~FdChannel() {

}

void FdChannel::Close() {
	if (event_) {
		if (attached_) {
			evloop::EventDel(event_);
		}
		
		delete event_;
		event_ = nullptr;
	}
	
	read_fn_ = ReadEventCallback();
	write_fn_ = EventCallback();
}

void FdChannel::AttachToLoop() {
	//LOG_T_F(LS_VERBOSE) << "fd=" << fd_ << " attach to event loop";
	assert(!IsNoneEvent());
	assert(loop_->IsInLoopThread());
	// detach firstly, avoid multiplying calling this
	if (attached_) {
		DetachFromLoop();
	}
	
	event_set(event_, fd_, flags_ | EV_PERSIST,
			&FdChannel::HandleEvent, this);
	event_base_set(loop_->event_base(), event_);
	
	if (evloop::EventAdd(event_, nullptr) != 0) {
		//LOG_T_F(LS_ERROR) << "event_add failed. fd=" << this->event_->ev_fd << " event_=" << event_;
		return;
	}
	
	attached_ = true;
}

void FdChannel::EnableReadEvent() {
	//LOG_T_F(LS_VERBOSE) << "fd=" << fd_ << " enable read event";
	int cur_flags = flags_;
	flags_ |= kReadable;
	
	if (flags_ != cur_flags) {
		UpdateFlag();
	}
}

void FdChannel::EnableWriteEvent() {
	//LOG_T_F(LS_VERBOSE) << "fd=" << fd_ << " enable write event";
	int cur_flags = flags_;
	flags_ |= kWritable;
	
	if (flags_ != cur_flags) {
		UpdateFlag();
	}
}

void FdChannel::DisableReadEvent() {
	int cur_flags = flags_;
	flags_ &= ~kReadable;
	
	if (flags_ != cur_flags) {
		UpdateFlag();
	}
}

void FdChannel::DisableWriteEvent() {
	int cur_flags = flags_;
	flags_ &= ~kWritable;
	
	if (flags_ != cur_flags) {
		UpdateFlag();
	}
}

void FdChannel::DisableAllEvent() {
	if (flags_ == kNone) {
		return;
	}
	
	flags_ = kNone;
	UpdateFlag();
}

void FdChannel::DetachFromLoop() {
	assert(loop_->IsInLoopThread());
	assert(attached_);
	
	if (evloop::EventDel(event_) != 0) {
		//LOG_T_F(LS_ERROR) << "DetachFromLoop this=" << this << "fd=" << fd_ << " with event " << EventsToString() << " detach from event loop failed";
		return;
	}
	else {
		//LOG_T_F(LS_VERBOSE) << "fd=" << fd_ << " detach from event loop";
		attached_ = false;
	}
}

void FdChannel::UpdateFlag() {
	if (IsNoneEvent()) {
		DetachFromLoop();
	}
	else {
		AttachToLoop();
	}
}

std::string FdChannel::EventsToString() const {
	std::string s;
	
	if (flags_ & kReadable) {
		s += "kReadable";
	}
	
	if (flags_ & kWritable) {
		if (!s.empty()) {
			s += " | ";
		}
		
		s += "kWritable";
	}
	
	return s;
}

void FdChannel::HandleEvent(int fd, short which) {
	assert(fd_ == fd);
	
	if ((which & kReadable) && read_fn_) {
		read_fn_();
	}
	
	if ((which & kWritable) && write_fn_) {
		write_fn_();
	}
}

void FdChannel::HandleEvent(int fd, short which, void* v) {
	FdChannel *c = (FdChannel *)v;
	c->HandleEvent(fd, which);
}


} // namespace evloop
