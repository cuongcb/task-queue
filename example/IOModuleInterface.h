/* 
 * File:   IOModuleInterface.h
 * Author: lap11894
 *
 * Created on February 19, 2019, 11:18 AM
 */

#ifndef ZRTC_IOMODULEINTERFACE_H
#define ZRTC_IOMODULEINTERFACE_H

#include <functional>

#include "zrtc/common/Common.h"

BEG_NSP_ZRTC();

typedef std::function<void(const uint8_t *, uint32_t)> OnReceiveCallback;

class IOModuleInterface {
public:
	virtual ~IOModuleInterface() {
		
	}

	virtual void Start() = 0;
	virtual void Stop() = 0;
	virtual void Send(const uint8_t *data, uint32_t len) = 0;
	virtual void RegisterOnReceiveCallback(OnReceiveCallback cb) = 0;
};

END_NSP_ZRTC();

#endif /* ZRTC_IOMODULEINTERFACE_H */

