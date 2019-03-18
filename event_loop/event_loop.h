/* 
 * File:   event_loop.h
 * Author: lap11894
 *
 * Created on January 5, 2019, 2:44 PM
 */

#ifndef ZRTC_EVENT_LOOP_H
#define ZRTC_EVENT_LOOP_H

#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

#include "zrtc/event_loop/event_status.h"
#include "zrtc/event_loop/invoke_timer.h"

struct event_base;

namespace evloop {

class EventWatcher;

class EventLoop: public EventStatus {
public:
	typedef std::function<void()> Functor;
	
public:
	EventLoop();
	
	// Construct EventLoop from an existing event_base object
	// Possibly embed EventLoop into the current event_base structure
	explicit EventLoop(struct ::event_base *base);
	
	~EventLoop();
	
	// @brief: Run the io event loop forever
	// @note: It must be called in the io event thread
	void Run();
	
    InvokeTimerPtr RunAfter(int delay_ms, const Functor &f);
	InvokeTimerPtr RunAfter(int delay_ms, Functor &&f);
	
	InvokeTimerPtr RunEvery(int time_ms, const Functor &f);
	InvokeTimerPtr RunEvery(int time_ms, Functor &&f);
	
	// @brief: Stop the event loop
	void Stop();
	
	void RunInLoop(const Functor &f);
	void QueueInLoop(const Functor &f);
	
	void RunInLoop(Functor &&f);
	void QueueInLoop(Functor &&f);
	
public:
	struct event_base *event_base() const {
		return evbase_;
	}
	
	bool IsInLoopThread() const {
		return tid_ == std::this_thread::get_id();
	}
	
	int pending_functor_count() const {
		return pending_functor_count_.load();
	}
	
	const std::thread::id & tid() const {
		return tid_;
	}
	
private:
	// @brief: Error-prone initialization goes here
	void Init();
	
	// @brief: Create self-pipe watcher
	void InitNotifyPipeWatcher();
	
	// @brief: Stop the event loop in the io event thread
	void StopInLoop();
	
	void DoPendingFunctors();
	
	size_t GetPendingQueueSize();
	
	bool IsPendingQueueEmpty();
	
private:
	struct event_base *evbase_;
	bool create_evbase_myself_;
	
	std::thread::id tid_;
	
	std::mutex mutex_;
	
	// Used to notify the thread when we push a task into queue
	std::unique_ptr<EventWatcher> watcher_;
	
	// Avoid notifying repeatedly when pushing tasks
	std::atomic<bool> notified_;
	
	std::vector<Functor> *pending_functors_; // guard by mutex_
	
	std::atomic<int> pending_functor_count_;
};

} // namespace evloop

#endif /* EVENT_LOOP_H */

