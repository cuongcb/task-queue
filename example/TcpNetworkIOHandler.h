#ifndef ZRTC_TCPNETWORKIOHANDLER_H
#define ZRTC_TCPNETWORKIOHANDLER_H

#include "webrtc/base/basictypes.h"
#include "webrtc/base/asyncpacketsocket.h"
#include "zrtc/common/Common.h"

BEG_NSP_ZRTC();

class TcpNetworkIOHandler {
public:
	virtual void OnReadTcpPacket(const rtc::PacketTime &recv_time,
								uint8_t *data,
								size_t size) = 0;
	
	virtual void OnReadTcpPacketPreConnect(uint8_t *data,
											size_t size,
											bool pre_incoming) = 0;
	
	virtual void OnNetworkError(int32_t error) = 0;

	virtual void OnResetSocket(int32_t error, int32_t time) = 0;
	
	virtual void OnEstablishConnection(bool success) = 0;
	
	virtual void OnCloseConnection() = 0;
};

END_NSP_ZRTC();

#endif // ZRTC_TCPNETWORKIOHANDLER_H