#include "zrtc/event_loop/invoke_timer.h"

#include <memory>

#include "zrtc/event_loop/event_loop.h"
#include "zrtc/event_loop/event_watcher.h"

namespace evloop {

InvokeTimer::InvokeTimer(EventLoop* evloop, int timeout_ms, const Functor& f, bool periodic)
    : loop_(evloop), timeout_ms_(timeout_ms), functor_(f), periodic_(periodic) {
    LOG_T_F(LS_INFO) << "loop=" << loop_;
}

InvokeTimer::InvokeTimer(EventLoop* evloop, int timeout_ms, Functor&& f, bool periodic)
    : loop_(evloop), timeout_ms_(timeout_ms), functor_(std::move(f)), periodic_(periodic) {
    LOG_T_F(LS_INFO) << "loop=" << loop_;
}

InvokeTimerPtr InvokeTimer::Create(EventLoop* evloop, int timeout_ms, const Functor& f, bool periodic) {
    InvokeTimerPtr it(new InvokeTimer(evloop, timeout_ms, f, periodic));
    it->self_ = it;
    return it;
}

InvokeTimerPtr InvokeTimer::Create(EventLoop* evloop, int timeout_ms, Functor&& f, bool periodic) {
    InvokeTimerPtr it(new InvokeTimer(evloop, timeout_ms, std::move(f), periodic));
    it->self_ = it;
    return it;
}

InvokeTimer::~InvokeTimer() {
    LOG_T_F(LS_INFO) << "loop=" << loop_;
}

void InvokeTimer::Start() {
    LOG_T_F(LS_INFO) << "loop=" << loop_ << " refcount=" << self_.use_count();

    auto f = [this]() {
		{
			auto time_weak = std::weak_ptr<InvokeTimer>(shared_from_this());
			timer_.reset(new TimerEventWatcher(loop_, [time_weak]() {
				auto time_ptr = time_weak.lock();
				if (time_ptr) {
					time_ptr->OnTimerTriggered();
				}
			}, timeout_ms_));
		}
        
		{
			auto time_weak = std::weak_ptr<InvokeTimer>(shared_from_this());
			timer_->SetCancelCallback([time_weak]() {
				auto time_ptr = time_weak.lock();
				if (time_ptr) {
					time_ptr->OnCanceled();
				}
			});
		}
		
		timer_->Init();
        timer_->AsyncWait();
        
        LOG_T_F(LS_VERBOSE) << "timer=" << timer_.get() << " loop=" << loop_ << " refcount=" << self_.use_count() << " periodic=" << periodic_ << " timeout(ms)=" << timeout_ms_;
    };
    loop_->RunInLoop(std::move(f));
}

void InvokeTimer::Cancel() {
	auto time_weak = std::weak_ptr<InvokeTimer>(shared_from_this());
    auto f = [time_weak]() {
        auto time_ptr = time_weak.lock();
        if (time_ptr && time_ptr->timer_) {
            time_ptr->timer_->Cancel();
        }
    };
    loop_->RunInLoop(std::move(f));
}

void InvokeTimer::OnTimerTriggered() {
    LOG_T_F(LS_INFO) << "loop=" << loop_ << " use_count=" << self_.use_count();
    functor_();

    if (periodic_) {
        timer_->AsyncWait();
    } else {
        timer_.reset();
        self_.reset();
    }
}

void InvokeTimer::OnCanceled() {
    LOG_T_F(LS_INFO) << "loop=" << loop_ << " use_count=" << self_.use_count();
    periodic_ = false;
    if (cancel_callback_) {
        cancel_callback_();
    }
    timer_.reset();
    self_.reset();
}

}


