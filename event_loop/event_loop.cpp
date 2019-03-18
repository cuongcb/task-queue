#include "zrtc/event_loop/event_loop.h"

#include "zrtc/event_loop/libevent.h"
#include "zrtc/event_loop/event_common.h"
#include "zrtc/event_loop/event_sockets.h"
#include "zrtc/event_loop/event_watcher.h"


namespace evloop {

EventLoop::EventLoop()
	: create_evbase_myself_(true), notified_(false), pending_functor_count_(0) {
	evbase_ = event_base_new();
	Init();
}

EventLoop::EventLoop(struct ::event_base* base)
	: create_evbase_myself_(false)
	, notified_(false)
	, pending_functor_count_(0) {
	Init();
	bool ret = watcher_->AsyncWait();
	if (!ret) {
		LOG_T_F(LS_ERROR) << "PipeEventWatcher init failed.";
	}

	status_ = kRunning;
}

EventLoop::~EventLoop() {
	watcher_.reset();
	
	if (evbase_ != nullptr && create_evbase_myself_) {
		event_base_free(evbase_);
		evbase_ = nullptr;
	}
	
	delete pending_functors_;
	pending_functors_ = nullptr;
}

void EventLoop::Init() {
	status_ = kInitializing;
	
	pending_functors_ = new std::vector<Functor>();
	
	tid_ = std::this_thread::get_id();
	
	InitNotifyPipeWatcher();
	
	status_ = kInitialized;
}

void EventLoop::InitNotifyPipeWatcher() {
	watcher_.reset(new PipeEventWatcher(this,
					std::bind(&EventLoop::DoPendingFunctors, this)));
	bool ret = watcher_->Init();
	if (!ret) {
		LOG_T_F(LS_ERROR) << "PipeEventWatcher init failed.";
	}
}

void EventLoop::Run() {
	//LOG_T_F(LS_INFO) << "";
	status_ = kStarting;
	tid_ = std::this_thread::get_id();
	
	bool ret = watcher_->AsyncWait();
	if (!ret) {
		//LOG_T_F(LS_ERROR) << "PipeEventWatcher init failed.";
	}
	
	status_ = kRunning;
	
	auto running = event_base_dispatch(evbase_);
	if (running == 1) {
		//LOG_T_F(LS_ERROR) << "event_base_dispatch error: no event registered";
	}
	else if (running == -1) {
		int err = errno;
		//LOG_T_F(LS_ERROR) << "event_base_dispatch error " << err << " " << strerror(err);
	}
	
	// out of event dispatch loop
	watcher_.reset();
	//LOG_T_F(LS_INFO) << "EventLoop stopped, tid=" << std::this_thread::get_id();
	status_ = kStopped;
}

InvokeTimerPtr EventLoop::RunAfter(int delay_ms, const Functor &f) {
	//LOG_T_F(LS_INFO) << "";
	std::shared_ptr<InvokeTimer> t = InvokeTimer::Create(this, delay_ms, f, false);
    t->Start();
    return t;
}

InvokeTimerPtr EventLoop::RunAfter(int delay_ms, Functor &&f) {
	//LOG_T_F(LS_INFO) << "";
	std::shared_ptr<InvokeTimer> t = InvokeTimer::Create(this, delay_ms, std::move(f), false);
    t->Start();
    return t;
}

InvokeTimerPtr EventLoop::RunEvery(int time_ms, const Functor &f) {
	//LOG_T_F(LS_INFO) << "";
	std::shared_ptr<InvokeTimer> t = InvokeTimer::Create(this, time_ms, f, true);
	t->Start();
	return t;
}

InvokeTimerPtr EventLoop::RunEvery(int time_ms, Functor &&f) {
	//LOG_T_F(LS_INFO) << "";
	std::shared_ptr<InvokeTimer> t = InvokeTimer::Create(this, time_ms, std::move(f), true);
	t->Start();
	return t;
}

void EventLoop::Stop() {
	//LOG_T_F(LS_INFO) << "";
	assert(status_ == kRunning);
	status_ = kStopping;
	QueueInLoop(std::bind(&EventLoop::StopInLoop, this));
}

void EventLoop::StopInLoop() {
	//LOG_T_F(LS_INFO) << "";
	assert(status_ == kStopping);
	
	auto f = [this]() {
		while (!IsPendingQueueEmpty()) {
			DoPendingFunctors();
		}
	};
	
	f();
	
	event_base_loopexit(evbase_, nullptr);
	
	f();
}

void EventLoop::RunInLoop(const Functor& f) {
	//LOG_T_F(LS_INFO) << "";
	if (IsRunning() && IsInLoopThread()) {
		f();
	}
	else {
		QueueInLoop(f);
	}
}

void EventLoop::RunInLoop(Functor&& f) {
	//LOG_T_F(LS_INFO) << "";
	if (IsRunning() && IsInLoopThread()) {
		f();
	}
	else {
		QueueInLoop(std::move(f));
	}
}

void EventLoop::QueueInLoop(const Functor &f) {
	//LOG_T_F(LS_INFO) << "";
	{
		std::lock_guard<std::mutex> lock(mutex_);
		pending_functors_->emplace_back(f);
	}
	
	++pending_functor_count_;
	
	if (!notified_) {
		notified_ = true;
		if (watcher_.get()) {
			watcher_->Notify();
		}
		else {
			//LOG_T_F(LS_INFO) << "status=" << StatusToString();
			assert(!IsRunning());
		}
	}
}

void EventLoop::QueueInLoop(Functor&& f) {
	//LOG_T_F(LS_INFO) << "";
	{
		std::lock_guard<std::mutex> lock(mutex_);
		pending_functors_->emplace_back(std::move(f));
	}
	
	++pending_functor_count_;
	
	if (!notified_) {
		notified_ = true;
		if (watcher_.get()) {
			watcher_->Notify();
		}
		else {
			//LOG_T_F(LS_INFO) << "status=" << StatusToString();
			assert(!IsRunning());
		}
	}
}

void EventLoop::DoPendingFunctors() {
	std::vector<Functor> functors;
	{
		std::lock_guard<std::mutex> lock(mutex_);
		notified_ = false;
		pending_functors_->swap(functors);
	}
	
	for (std::vector<Functor>::size_type i = 0; i < functors.size(); ++i) {
		//LOG_T_F(LS_INFO) << "Doing functor #" << i;
		functors[i]();
		--pending_functor_count_;
	}
}

size_t EventLoop::GetPendingQueueSize() {
	return pending_functors_->size();
}

bool EventLoop::IsPendingQueueEmpty() {
	return pending_functors_->empty();
}

} // namespace evloop