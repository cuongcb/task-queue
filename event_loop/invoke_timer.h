/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/* 
 * File:   invoke_timer.h
 * Author: lap11894
 *
 * Created on January 11, 2019, 3:59 PM
 */

#ifndef ZRTC_INVOKE_TIMER_H
#define ZRTC_INVOKE_TIMER_H

#include "zrtc/event_loop/libevent.h"
#include "zrtc/event_loop/event_common.h"

namespace evloop {
	
class EventLoop;
class EventWatcher;
class InvokeTimer;

typedef std::shared_ptr<InvokeTimer> InvokeTimerPtr;

class InvokeTimer : public std::enable_shared_from_this<InvokeTimer> {
public:
    typedef std::function<void()> Functor;

    // @brief Create a timer. When the timer is timeout, the functor f will
    //  be invoked automatically.
    // @param evloop - The EventLoop runs this timer
    // @param timeout - The timeout when the timer is invoked
    // @param f -
    // @param periodic - To indicate this timer whether it is a periodic timer.
    //  If it is true this timer will be automatically invoked periodic.
    // @return evpp::InvokeTimerPtr - The user layer can hold this shared_ptr
    //  and can cancel this timer at any time.
    static InvokeTimerPtr Create(EventLoop* evloop,
                                 int timeout_ms,
                                 const Functor& f,
                                 bool periodic);
    static InvokeTimerPtr Create(EventLoop* evloop,
                                 int timeout_ms,
                                 Functor&& f,
                                 bool periodic);
    ~InvokeTimer();

    // It is thread safe.
    // Start this timer.
    void Start();

    // Cancel the timer and the cancel_callback_ will be invoked.
    void Cancel();

    void set_cancel_callback(const Functor& fn) {
        cancel_callback_ = fn;
    }
private:
    InvokeTimer(EventLoop* evloop, int timeout_ms, const Functor& f, bool periodic);
    InvokeTimer(EventLoop* evloop, int timeout_ms, Functor&& f, bool periodic);
    void OnTimerTriggered();
    void OnCanceled();

private:
    EventLoop* loop_;
    int timeout_ms_;
    Functor functor_;
    Functor cancel_callback_;
    std::unique_ptr<EventWatcher> timer_;
    bool periodic_;
    std::shared_ptr<InvokeTimer> self_; // Hold myself
};

} // namespace evloop

#endif /* ZRTC_INVOKE_TIMER_H */

