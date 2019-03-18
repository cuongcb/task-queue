/* 
 * File:   TcpCallback.h
 * Author: lap11894
 *
 * Created on January 7, 2019, 3:22 PM
 */

#ifndef ZRTC_TCPCALLBACK_H
#define ZRTC_TCPCALLBACK_H

#include <functional>
#include <memory>

#include "zrtc/network/TcpBuffer.h"

namespace evloop {

class TcpConn;

typedef std::shared_ptr<TcpConn> TcpConnPtr;

typedef std::function<void(const TcpConnPtr &)>
		ConnectionCallback;
typedef std::function<void(const TcpConnPtr &, const zrtc::TcpBuffer::Ptr &)>
		WriteCompleteCallback;
typedef std::function<void(const TcpConnPtr &)>
		WriteReadyCallback;
typedef std::function<void(const TcpConnPtr &)>
		CloseCallback;
typedef std::function<void(const TcpConnPtr &, uint8_t * data, size_t len)> 
		MessageCallback;

namespace internalcb {
	inline void DefaultConnectionCallback(const TcpConnPtr &conn) {
		
	}
	
	inline void DefaultMessageCallback(const TcpConnPtr &conn,
										const uint8_t *buf,
										size_t len) {
		
	}
}

} // namespace evloop

#endif /* ZRTC_TCPCALLBACK_H */

