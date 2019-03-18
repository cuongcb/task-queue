/* 
 * File:   event_watcher.h
 * Author: lap11894
 *
 * Created on January 5, 2019, 4:16 PM
 */

#ifndef ZRTC_EVENT_WATCHER_H
#define ZRTC_EVENT_WATCHER_H

#include <functional>

struct event_base;
struct event;

namespace evloop {

class EventLoop;

class EventWatcher {
public:
	typedef std::function<void()> Handler;
	
public:
//	EventWatcher();
	virtual ~EventWatcher();
	
	bool Init();
	
	// @note: It must be called in the event thread
	void Cancel();
	
	void SetCancelCallback(const Handler &cb);
	
	void ClearHandler() {
		handler_ = Handler();
	}
	
	virtual bool AsyncWait() = 0;
	virtual void Notify() { } 
	
protected:
	// @note: It must be called in the event thread
	// @param timeout: the maximum amount of time to wait for the event,
	// or 0 to wait forever
	bool Watch(int timeout);
	
protected:
	EventWatcher(struct ::event_base *evbase, const Handler &handler);
	EventWatcher(struct ::event_base *evbase, Handler &&handler);
	
	void Close();
	void FreeEvent();
	
	virtual bool DoInit() = 0;
	virtual void DoClose() {}
	
protected:
	struct event *event_;
	struct event_base *evbase_;
	bool attached_;
	Handler handler_;
	Handler cancel_callback_;
};

class PipeEventWatcher: public EventWatcher {
public:
	PipeEventWatcher(EventLoop *loop, const Handler &handler);
	PipeEventWatcher(EventLoop *loop, Handler &&handler);
	
	virtual ~PipeEventWatcher();
	
	virtual bool AsyncWait() override;
	virtual void Notify() override;
	
	int wfd() const {
		return pipe_[0];
	}
	
private:
	virtual bool DoInit() override;
	virtual void DoClose() override;

	static void HandlerFn(int fd, short which, void *v);
	
private:
	int pipe_[2];
};

class TimerEventWatcher: public EventWatcher {
public:
	TimerEventWatcher(EventLoop *loop, const Handler &handler, int timeout);
	TimerEventWatcher(EventLoop *loop, Handler &&handler, int timeout);
	
	virtual ~TimerEventWatcher();
	
	virtual bool AsyncWait() override;
	
private:
	virtual bool DoInit() override;
	
	static void HandlerFn(int fd, short which, void *v);
	
private:
	int timeout_ms_;
};
	
} // namespace evloop

#endif /* EVENT_WATCHER_H */

