/* 
 * File:   TcpChannel.h
 * Author: lap11894
 *
 * Created on January 7, 2019, 11:20 AM
 */

#ifndef ZRTC_FDCHANNEL_H
#define ZRTC_FDCHANNEL_H

#include <functional>
#include <string>

struct event;
struct event_base;

namespace evloop {
	
class EventWatcher;
class EventLoop;

class FdChannel {
public:
	enum EventType {
		kNone = 0x00,
		kReadable = 0x02,
		kWritable = 0x04,
	};
	
	typedef std::function<void()> EventCallback;
	typedef std::function<void()> ReadEventCallback;
	
public:
	FdChannel(EventLoop *loop, int fd,
			bool watch_read_event,
			bool watch_write_event);
	
	~FdChannel();
	
	void Close();
	
	void AttachToLoop();
	
	bool attached() const {
		return attached_;
	}
	
public:
	bool IsReadable() const {
		return (flags_ & kReadable) != 0;
	}
	
	bool IsWritable() const {
		return (flags_ & kWritable) != 0;
	}
	
	bool IsNoneEvent() const {
		return flags_ == kNone;
	}
	
	void EnableReadEvent();
	void EnableWriteEvent();
	void DisableReadEvent();
	void DisableWriteEvent();
	void DisableAllEvent();
	
public:
	int fd() const {
		return fd_;
	}
	
	std::string EventsToString() const;
	
public:
	void SetReadCallback(const ReadEventCallback &cb) {
		read_fn_ = cb;
	}
	
	void SetWriteCallback(const EventCallback &cb) {
		write_fn_ = cb;
	}
	
private:
	void HandleEvent(int fd, short which);
	static void HandleEvent(int fd, short which, void *v);
	
	void UpdateFlag();
	void DetachFromLoop();
	
private:
	ReadEventCallback read_fn_;
	EventCallback write_fn_;
	
	EventLoop * loop_;
	bool attached_;
	
	struct event * event_;
	int flags_;
	
	int fd_;
};

} // namespace evloop

#endif /* ZRTC_FDCHANNEL_H */

