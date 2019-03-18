/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/* 
 * File:   event_loop_utility.h
 * Author: lap11894
 *
 * Created on January 7, 2019, 9:15 AM
 */

#ifndef ZRTC_EVENT_LOOP_UTILITY_H
#define ZRTC_EVENT_LOOP_UTILITY_H

#ifdef __cplusplus
#include <cassert>
#include <cstdint>
#include <iostream>
#include <memory>
#include <functional>
#include <vector>

#endif // end of define __cplusplus

#include "zrtc/webrtc/base/logging.h"

struct event;

namespace evloop {
	int EventAdd(struct event *ev, const struct timeval *timeout);
	int EventDel(struct event *ev);
} // namespace evloop

#endif /* ZRTC_EVENT_LOOP_UTILITY_H */

