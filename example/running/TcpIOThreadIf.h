#ifndef ZRTCTCPIOTHREADIF_H
#define ZRTCTCPIOTHREADIF_H

#include "zrtc/network/TcpNetworkIOHandler.h"

BEG_NSP_ZRTC();

class TcpIOThreadIf {
public:
	virtual ~TcpIOThreadIf() {

	}

	virtual bool UpdateConnections(const std::string &host, uint32_t port) = 0;
	virtual bool CreateConnection() = 0;
	virtual bool CreateConnection(const std::string &host, uint32_t port) = 0;
	virtual bool CreateAsyncConnection(const std::string &host,
										uint32_t port) = 0;
	virtual void Start() = 0;
	virtual void Stop() = 0;
	virtual void Reset() = 0;
	virtual void RegisterNetworkHandler(TcpNetworkIOHandler *handler) = 0;
	virtual bool SendData(const uint8_t *data, size_t size) = 0;
	virtual int32_t InputBwKbit() = 0;
	virtual int32_t OutputBwKbit() = 0;
};

END_NSP_ZRTC();

#endif // ZRTCTCPIOTHREADIF_H