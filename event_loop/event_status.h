/* 
 * File:   status.h
 * Author: lap11894
 *
 * Created on January 5, 2019, 3:04 PM
 */

#ifndef ZRTC_EVENTSTATUS_H
#define ZRTC_EVENTSTATUS_H

#include <atomic>
#include <string>

namespace evloop {

class EventStatus {
public:
	enum eStatus {
		kNull = 0,
		kInitializing = 1,
		kInitialized = 2,
		kStarting = 3,
		kRunning = 4,
		kStopping = 5,
		kStopped = 6,
	};

	enum eSubStatus {
		kSubStatusNull = 0,
		kStoppingListener = 1,
		kStoppingThreadPool = 2,
	};
	
	EventStatus(): status_(kNull),
				substatus_(kSubStatusNull) {}
	
	std::string StatusToString() const {
		switch (status_) {
			case kNull:
				return "kNull";
			case kInitialized:
				return "kInitialized";
			case kRunning:
				return "kRunning";
			case kStopping:
				return "kStopping";
			case kStopped:
				return "kStopped";
			default:
				return "Unknown";
		}
	}
	
public:
	bool IsRunning() const {
		return status_ == kRunning;
	}
	
	bool IsStopped() const {
		return status_ == kStopped;
	}
	
	bool IsStopping() const {
		return status_ == kStopping;
	}
	
protected:
	~EventStatus() {}
	
protected:
	std::atomic<eStatus> status_;
	std::atomic<eSubStatus> substatus_;
};

} // namespace evloop

#endif /* STATUS_H */

